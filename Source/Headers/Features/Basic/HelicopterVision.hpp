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

	uint32_t currentColourValue = 0x0;





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



	void __fastcall UpdateColourBySight
	(
		const address copAIVehicle,
		const bool    canSeeTarget
	) {
		static constinit float visionState         = 0.f; // out-of-sight (0) to within-sight (1)
		static constinit float lastUpdateTimestamp = 0.f; // seconds

		bool& isKnownVehicle = AsReference<bool>(copAIVehicle - 0x4C + 0x769); // padding byte

		const float timestamp = Globals::GetGameplayTime();

		if (isKnownVehicle)
		{
			// Guard against potential wrap-around / reset
			const float timeDelta = std::max<float>(timestamp - lastUpdateTimestamp, 0.f);

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

		currentColourValue  = InterpolateColour(visionState);
		lastUpdateTimestamp = timestamp;
	}



	void __fastcall ApplyColour(const address interfaceObject)
	{
		const auto SetFEngColour = AsFunction<void __cdecl (address, uint32_t)>(0x5157E0);
		SetFEngColour(interfaceObject, currentColourValue); // persists until overridden
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Updates the helicopter's vision-cone colour and applies it to the icon
	ASSEMBLY_DETOUR(ColourUpdate, /* begin = */ 0x579FC6, /* end = */ 0x579FCB)
	{
		__asm
		{
			mov dword ptr [currentColourValue], 0x0 // invisible

			mov ecx, dword ptr [esi]
			call Globals::IsVehicleDestroyed
			test al, al
			jne colour // helicopter destroyed

			mov dword ptr [currentColourValue], 0xFF90B8FF // vanilla

			cmp byte ptr [anyFeatureEnabled], 1
			jne colour // cone feature disabled

			mov eax, dword ptr [esi]
			mov edx, dword ptr [eax + 0x54] // AIVehicle
			push edx

			push dword ptr [edx + 0x54] // target
			mov ecx, edi
			mov edx, dword ptr [edi]
			call dword ptr [edx + 0x7C] // AIVehicleHelicopter::CanSeeTarget

			pop ecx
			movzx edx, al
			call UpdateColourBySight // ecx: copAIVehicle, edx: canSeeTarget

			colour:
			mov ecx, dword ptr [ebx + 0xCC]
			call ApplyColour // ecx: interfaceObject

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

			mov ecx, dword ptr [esi + 0x3C]
			call ApplyColour // ecx: interfaceObject

			xor eax, eax // restore zero flag

			conclusion:
			EXIT_ASSEMBLY_DETOUR(WorldMapIcon)
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



	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileCosmetic)) return false;

		// Cone colours
		if (not ExtractColours(parser)) return false; // invalid colours; disable feature

		// Code modifications
		PATCH_ASSEMBLY_DETOUR(WorldMapIcon);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}