#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <cstdint>
#include <algorithm>
#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace HelicopterVision
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[VIS]";
	constexpr Globals::LogLiteral logName = "HelicopterVision";

	// Types and aliases
	constexpr size_t numChannels = 4;

	template <typename T>
	using ARGB = std::array<T, numChannels>; // alpha, red, green, blue

	struct Colour
	{
	// Members

		ARGB<float> channels = {};

		float transitionLength = .2f; // seconds
	};

	// Assembly detours
	constinit Colour outOfSight;
	constinit Colour withinSight;

	uint32_t currentColour = 0x0;

	bool isNewWorldMap = false;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] uint32_t InterpolateColour(const float state)
	{
		uint32_t colour = 0x0; // format: 0xAARRGGBB

		const auto& min = outOfSight .channels;
		const auto& max = withinSight.channels;

		for (size_t channelID = 0; channelID < numChannels; ++channelID)
			colour = (colour << 8) | static_cast<byte>(std::lerp(min[channelID], max[channelID], state));

		return colour;
	}



	[[nodiscard]] uint32_t YieldColourBySight
	(
		const address copAIVehicle,
		const bool    canSeeTarget
	) {
		static constinit float visionState         = 0.f; // out-of-sight (0) to within-sight (1)
		static constinit float lastUpdateTimestamp = 0.f; // seconds

		const float timestamp = Globals::GetGameplayTime();

		if (bool& isKnownVehicle = AsReference<bool>(copAIVehicle - 0x4C + 0x769)) // padding byte
		{
			const float timeDelta = std::max<float>(timestamp - lastUpdateTimestamp, 0.f); // guard against wrap-around / reset

			const float deltaDirection = (canSeeTarget) ? 1.f         : -1.f;
			const auto& targetColour   = (canSeeTarget) ? withinSight : outOfSight;
			
			visionState += deltaDirection * timeDelta / targetColour.transitionLength;
			visionState  = std::clamp<float>(visionState, 0.f, 1.f);
		}
		else
		{
			isKnownVehicle = true;
			visionState    = 0.f;
		}

		lastUpdateTimestamp = timestamp;

		return InterpolateColour(visionState);
	}



	[[nodiscard]] uint32_t __fastcall GetNewColour(const address copVehicle)
	{
		if (Globals::IsVehicleDestroyed(copVehicle)) return 0x0;        // invisible
		if (not anyFeatureEnabled)                   return 0xFF90B8FF; // vanilla

		const address copAIVehicle        = Globals::GetAIVehicleOfVehicle       (copVehicle);
		const address copAIVehiclePursuit = Globals::GetAIVehiclePursuitOfVehicle(copVehicle);

		const auto    GetPursuitTarget = AsFunction<address __thiscall (address)>(0x409860);
		const address pursuitTarget    = GetPursuitTarget(copAIVehiclePursuit);

		const auto CanSeeTarget = AsFunction<bool __thiscall (address, address)>(0x4170D0);
		const bool canSeeTarget = (pursuitTarget and CanSeeTarget(copAIVehiclePursuit, pursuitTarget));

		return YieldColourBySight(copAIVehicle, canSeeTarget);
	}



	void __fastcall ApplyCurrentColour(const address element)
	{
		const auto SetFEngColour = AsFunction<void __cdecl (address, uint32_t)>(0x5157E0);
		SetFEngColour(element, currentColour); // persists until next SetFEngColour call
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Updates the helicopter's vision-cone colour and applies it to the icon
	ASSEMBLY_DETOUR(ColourUpdate, /* begin = */ 0x579FC6, /* end = */ 0x579FCB)
	{
		__asm
		{
			mov ecx, dword ptr [esi]
			call GetNewColour // ecx: copVehicle
			cmp dword ptr [currentColour], eax
			je conclusion     // colour unchanged

			mov dword ptr [currentColour], eax

			mov ecx, dword ptr [ebx + 0xCC]
			call ApplyCurrentColour // ecx: element

			conclusion:
			// Execute original code and resume
			mov byte ptr [esp + 0x13], 1

			EXIT_ASSEMBLY_DETOUR(ColourUpdate)
		}
	}



	// Applies the helicopter's vision-cone colour on the world map
	ASSEMBLY_DETOUR(WorldMapIcon, 0x51F736, 0x51F73B)
	{
		__asm
		{
			// Execute original code first
			cmp byte ptr [esi + 0x34], 0
			jne conclusion // skip drawing icon

			cmp byte ptr [isNewWorldMap], 0
			je conclusion

			mov ecx, dword ptr [esi + 0x3C]
			call ApplyCurrentColour // ecx: element

			mov byte ptr [isNewWorldMap], 0

			xor eax, eax // restore zero flag

			conclusion:
			EXIT_ASSEMBLY_DETOUR(WorldMapIcon)
		}
	}



	// Updates the world-map flag when a new world map is created
	ASSEMBLY_DETOUR(WorldMapConstructor, 0x561505, 0x56150B)
	{
		__asm
		{
			mov byte ptr [isNewWorldMap], 1

			// Execute original code and resume
			mov dword ptr [esi + 0x128], eax

			EXIT_ASSEMBLY_DETOUR(WorldMapConstructor)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool ExtractColour
	(
		const auto&            section,
		const std::string_view colourName,
		Colour&                colour
	) {
		constexpr ConfigParser::Bounds<int> limits(0, std::numeric_limits<byte>::max());

		ARGB<int> rawChannels = {};

		const bool allExtracted = ConfigParser::Parser::ExtractScalars<int, int, int, int, float>
		(
			section,
			colourName,
			{rawChannels[1],          limits}, // red
			{rawChannels[2],          limits}, // green
			{rawChannels[3],          limits}, // blue
			{rawChannels[0],          limits}, // alpha
			{colour.transitionLength, {.001f}}
		);

		if (not allExtracted) return false;

		for (size_t channelID = 0; channelID < numChannels; ++channelID)
			colour.channels[channelID] = static_cast<float>(rawChannels[channelID]);

		return true;
	}



	[[nodiscard]] bool ExtractColours(const ConfigParser::Parser& parser)
	{
		const auto* const section = parser.GetSection("Helicopter:Vision");

		// Out-of-sight colour
		if (not ExtractColour(section, "outOfSight", outOfSight))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("No valid out-of-sight colour");

			return false; // invalid colour
		}

		// Within-sight colour
		if (not ExtractColour(section, "withinSight", withinSight))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("No valid within-sight colour");

			return false; // invalid colour
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogPlain("Out of sight:", InterpolateColour(0.f), outOfSight .transitionLength);
			Globals::LogPlain("Within sight:", InterpolateColour(1.f), withinSight.transitionLength);
		}

		return true;
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Visible cone icon for destroyed helicopter
		PATCH_ASSEMBLY_DETOUR(ColourUpdate); 
	}



	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileCosmetic)) return false;

		// Cone colours
		if (not ExtractColours(parser)) return false; // invalid colours; disable feature

		// Code modifications
		PATCH_ASSEMBLY_DETOUR(WorldMapIcon);
		PATCH_ASSEMBLY_DETOUR(WorldMapConstructor);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}