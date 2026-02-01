#pragma once

#include <array>
#include <algorithm>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace HelicopterVision
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Data structures
	template <typename T>
	using BGRA = std::array<T, 4>; // blue, green, red, alpha

	// Code caves
	float lengthToBase = .2f; // seconds
	float lengthToEnd  = .2f; // seconds

	BGRA<float> baseColour  = {};
	BGRA<float> colourRange = {};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	uint32_t StateToColour(const float state)
	{
		uint32_t colour = 0x0;

		for (size_t channelID = 0; channelID < 4; ++channelID)
			colour |= static_cast<byte>(baseColour[channelID] + state * colourRange[channelID]) << (8 * channelID);
	
		return colour;
	}



	uint32_t __fastcall GetConeColour
	(
		const address heliAIVehicle,
		const bool    canSeeTarget
	) {
		static float currentColourState  = 0.f; // ratio
		static float lastUpdateTimestamp = 0.f; // seconds

		const float currentTimestamp = Globals::GetGameTime(true);

		volatile bool& isKnownHelicopter = *reinterpret_cast<volatile bool*>(heliAIVehicle - 0x4C + 0x769); // padding byte

		if (isKnownHelicopter)
		{
			const float timeDelta = currentTimestamp - lastUpdateTimestamp;

			if (canSeeTarget)
				currentColourState += timeDelta / lengthToEnd;

			else
				currentColourState -= timeDelta / lengthToBase;

			currentColourState = std::clamp<float>(currentColourState, 0.f, 1.f);
		}
		else
		{
			isKnownHelicopter  = true;
			currentColourState = (canSeeTarget) ? 1.f : 0.f;
		}

		lastUpdateTimestamp = currentTimestamp;

		return StateToColour(currentColourState);
	}



	bool ParseColour
	(
		HeatParameters::Parser& parser,
		const std::string&      colourName,
		BGRA<float>&            colour,
		float&                  length
	) {
		BGRA<int> rawColour = {};

		const HeatParameters::Bounds<int> limits(0, 255);

		const bool isValid = parser.ParseFromFile<int, int, int, int, float>
		(
			"Helicopter:Vision",
			colourName,
			{rawColour[2], limits}, // red
			{rawColour[1], limits}, // green
			{rawColour[0], limits}, // blue
			{rawColour[3], limits}, // alpha
			{length, {.001f}}
		);

		if (isValid)
		{
			for (size_t channelID = 0; channelID < 4; ++channelID)
				colour[channelID] = static_cast<float>(rawColour[channelID]);
		}

		return isValid;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address coneIconColourEntrance = 0x579FC6;
	constexpr address coneIconColourExit     = 0x579FCB;

	// Makes the colour of the helicopter cone-of-vision icon LOS-dependent
	__declspec(naked) void ConeIconColour()
	{
		static constexpr address FEngSetColour = 0x5157E0;

		__asm
		{
			mov ecx, dword ptr [esi]
			call Globals::IsVehicleDestroyed
			movzx eax, al
			dec eax
			je colour // helicopter destroyed

			mov eax, 0xFF90B8FF
			cmp byte ptr featureEnabled, 0x1
			jne colour // cone feature disabled

			mov eax, dword ptr [esi]
			mov edx, dword ptr [eax + 0x54] // AIVehicle
			push edx

			push dword ptr [edx + 0x54] // target
			mov ecx, edi
			mov edx, dword ptr [edi]
			call dword ptr [edx + 0x7C] // AIVehicleHelicopter::CanSeeTarget

			pop ecx
			mov dl, al
			call GetConeColour // ecx: heliAIVehicle, dl: canSeeTarget

			colour:
			push eax                    // colour
			push dword ptr [ebx + 0xCC] // vision-cone object
			call dword ptr FEngSetColour
			add esp, 0x8

			// Execute original code and resume
			mov byte ptr [esp + 0x13], 0x1

			jmp dword ptr coneIconColourExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Also fixes the helicopter cone icon staying visible if destroyed
		MemoryTools::MakeRangeJMP(ConeIconColour, coneIconColourEntrance, coneIconColourExit);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [VIS] HelicopterVision");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Parse and check in-sight colour
		if (not ParseColour(parser, "withinSightColour", colourRange, lengthToEnd))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Invalid within-sight colour");

			return false;
		}

		// Parse and check out-of-sight colour
		if (not ParseColour(parser, "outOfSightColour",  baseColour,  lengthToBase))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Invalid out-of-sight colour");

			return false;
		}

		for (size_t channelID = 0; channelID < 4; ++channelID)
			colourRange[channelID] -= baseColour[channelID];

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log<2>("Within sight:", StateToColour(1.f), lengthToEnd);
			Globals::logger.Log<2>("Out of sight:", StateToColour(0.f), lengthToBase);
		}

		// Code modifications
		ApplyFixes(); // also contains vision-cone feature

		// Status flag
		featureEnabled = true;

		return true;
	}
}