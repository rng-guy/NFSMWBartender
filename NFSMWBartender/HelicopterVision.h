#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace HelicopterVision
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Types and aliases
	constexpr size_t numChannels = 4;

	template <typename T>
	using ARGB = std::array<T, numChannels>; // alpha, red, green, blue

	struct Colour
	{
		ARGB<float> channels = {};

		float transitionLength = .2f; // seconds
	};

	// Code caves
	constinit Colour outOfSight;
	constinit Colour withinSight;

	uint32_t currentColour = 0x0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] uint32_t InterpolateColour(const float state)
	{
		uint32_t colour = 0x0;

		for (size_t channelID = 0; channelID < numChannels; ++channelID)
		{
			const float channel = std::lerp(outOfSight.channels[channelID], withinSight.channels[channelID], state);

			// Construct colour integer of format 0xAARRGGBB
			colour = (colour << 8) | static_cast<byte>(channel);
		}

		return colour;
	}



	void __fastcall UpdateColourBySight
	(
		const address copAIVehicle,
		const bool    canSeeTarget
	) {
		static constinit float currentVisionState  = 0.f; // out-of-sight (0) to within-sight (1)
		static constinit float lastUpdateTimestamp = 0.f; // seconds

		const float    currentTimestamp  = Globals::GetUnpausedGameTime();
		volatile bool& isKnownCopVehicle = AsVolatile<bool>(copAIVehicle - 0x4C + 0x769); // padding byte

		if (isKnownCopVehicle)
		{
			const float timeDelta = currentTimestamp - lastUpdateTimestamp;

			if (canSeeTarget)
				currentVisionState += timeDelta / withinSight.transitionLength;

			else
				currentVisionState -= timeDelta / outOfSight.transitionLength;

			currentVisionState = std::clamp<float>(currentVisionState, 0.f, 1.f);
		}
		else
		{
			isKnownCopVehicle  = true;
			currentVisionState = 0.f;
		}

		currentColour       = InterpolateColour(currentVisionState);
		lastUpdateTimestamp = currentTimestamp;
	}



	void __fastcall ApplyColour(const address interfaceObject)
	{
		const auto SetFEngColour = AsFunction<void __cdecl (address, uint32_t)>(0x5157E0);
		SetFEngColour(interfaceObject, currentColour); // persists until overridden with another call
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address colourUpdateEntrance = 0x579FC6;
	constexpr address colourUpdateExit     = 0x579FCB;

	// Updates the helicopter's vision-cone colour and applies it on the mini-map
	__declspec(naked) void ColourUpdate()
	{
		__asm
		{
			mov dword ptr [currentColour], 0x0 // invisible

			mov ecx, dword ptr [esi]
			call Globals::IsVehicleDestroyed
			test al, al
			jne colour // helicopter destroyed

			mov dword ptr [currentColour], 0xFF90B8FF // vanilla

			cmp byte ptr [featureEnabled], 1
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

			jmp dword ptr [colourUpdateExit]
		}
	}



	constexpr address worldMapIconEntrance = 0x51F736;
	constexpr address worldMapIconExit     = 0x51F73B;

	// Applies the helicopter's vision-cone colour on the world map
	__declspec(naked) void WorldMapIcon()
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
			jmp dword ptr [worldMapIconExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseColour
	(
		const HeatParameters::Parser& parser,
		const std::string_view        colourName,
		Colour&                       colour
	) {
		ARGB<int> rawChannels = {};

		constexpr HeatParameters::Bounds<int> limits(0, 255);

		const bool isValid = parser.ParseFromFile<int, int, int, int, float>
		(
			"Helicopter:Vision",
			colourName,
			{rawChannels[1], limits}, // red
			{rawChannels[2], limits}, // green
			{rawChannels[3], limits}, // blue
			{rawChannels[0], limits}, // alpha
			{colour.transitionLength, {.001f}}
		);

		if (isValid)
		{
			for (size_t channelID = 0; channelID < numChannels; ++channelID)
				colour.channels[channelID] = static_cast<float>(rawChannels[channelID]);
		}

		return isValid;
	}



	bool ParseColours(const HeatParameters::Parser& parser)
	{
		// Out-of-sight colour
		if (not ParseColour(parser, "outOfSight", outOfSight))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("No valid out-of-sight colour");

			return false; // invalid colour
		}

		// Within-sight colour
		if (not ParseColour(parser, "withinSight", withinSight))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("No valid within-sight colour");

			return false; // invalid colour
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log<2>("Out of sight:", InterpolateColour(0.f), outOfSight .transitionLength);
			Globals::logger.Log<2>("Within sight:", InterpolateColour(1.f), withinSight.transitionLength);
		}

		return true;
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Also fixes the helicopter cone icon staying visible if destroyed
		MemoryTools::MakeRangeJMP<colourUpdateEntrance, colourUpdateExit>(ColourUpdate);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [VIS] HelicopterVision");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Cone colours
		if (not ParseColours(parser)) return false; // invalid colours; disable feature

		// Code modifications
		ApplyFixes(); // also contains vision-cone feature for mini-map

		MemoryTools::MakeRangeJMP<worldMapIconEntrance, worldMapIconExit>(WorldMapIcon);

		// Status flag
		featureEnabled = true;

		return true;
	}
}