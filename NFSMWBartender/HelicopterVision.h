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

	// Code caves
	template <typename T>
	using BGRA = std::array<T, 4>;

	float lengthToBase = .2f;
	float lengthToEnd  = .2f;

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
		const address helicopterAIVehicle,
		const bool    canSeeTarget
	) {
		static float currentColourState  = 0.f;
		static float lastUpdateTimestamp = Globals::simulationTime;

		bool& isKnownVehicle = *reinterpret_cast<bool*>(helicopterAIVehicle - 0x4C + 0x769);

		if (isKnownVehicle)
		{
			const float timeDelta = Globals::simulationTime - lastUpdateTimestamp;

			if (canSeeTarget)
				currentColourState += timeDelta / lengthToEnd;

			else
				currentColourState -= timeDelta / lengthToBase;

			currentColourState = std::clamp<float>(currentColourState, 0.f, 1.f);
		}
		else
		{
			currentColourState = (canSeeTarget) ? 1.f : 0.f;
			isKnownVehicle     = true;
		}

		lastUpdateTimestamp = Globals::simulationTime;

		return StateToColour(currentColourState);
	}



	bool ParseColour
	(
		HeatParameters::Parser& parser,
		const char* const       colourName,
		BGRA<float>&            colour,
		float&                  length
	) {
		BGRA<int> rawColour = {};

		const bool isValid = parser.ParseParameterRow<int, int, int, int, float>
		(
			"Helicopter:Vision",
			colourName,
			{rawColour[2], 0, 255}, // red
			{rawColour[1], 0, 255}, // green
			{rawColour[0], 0, 255}, // blue
			{rawColour[3], 0, 255}, // alpha
			{length, .001f}
		);

		if (isValid)
		{
			for (size_t channelID = 0; channelID < 4; ++channelID)
				colour[channelID] = static_cast<float>(rawColour[channelID]);
		}

		return isValid;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address coneIconEntrance = 0x579FC6;
	constexpr address coneIconExit     = 0x579FCB;

	__declspec(naked) void ConeIcon()
	{
		static constexpr address updateFEngColour = 0x5157E0;

		__asm
		{
			mov edx, dword ptr [esi]
			mov ecx, dword ptr [edx + 0x54]
			test ecx, ecx
			je conclusion // invalid vehicle

			push ecx

			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x64]

			mov edx, dword ptr [esp]
			lea ecx, dword ptr [edx - 0x4C + 0x758]

			push eax
			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x7C]

			pop ecx
			mov dl, al
			call GetConeColour // ecx: helicopterAIVehicle, dl: canSeeTarget

			push eax
			push dword ptr [ebx + 0xCC]
			call dword ptr updateFEngColour
			add esp, 0x8

			conclusion:
			// Execute original code and resume
			mov byte ptr [esp + 0x13], 0x1

			jmp dword ptr coneIconExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		if (not ParseColour(parser, "withinSightColour", colourRange, lengthToEnd))  return false;
		if (not ParseColour(parser, "outOfSightColour",  baseColour,  lengthToBase)) return false;

		for (size_t channelID = 0; channelID < 4; ++channelID)
			colourRange[channelID] -= baseColour[channelID];

		// Code modifications
		MemoryTools::MakeRangeJMP(ConeIcon, coneIconEntrance, coneIconExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogSetupReport()
	{
		if (not featureEnabled) return;

		Globals::logger.Log("  CONFIG [VIS] HelicopterVision");

		Globals::logger.LogLongIndent("Within sight:", StateToColour(1.f), lengthToEnd);
		Globals::logger.LogLongIndent("Out of sight:", StateToColour(0.f), lengthToBase);
	}
}