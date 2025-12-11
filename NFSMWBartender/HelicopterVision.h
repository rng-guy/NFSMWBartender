#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace HelicopterVision
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves
	uint32_t withinSightColour = 0x0;
	uint32_t outOfSightColour  = 0x0;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address coneIconEntrance = 0x579FC6;
	constexpr address coneIconExit     = 0x579FCB;

	__declspec(naked) void ConeIcon()
	{
		static constexpr address updateFEngColour = 0x5157E0;

		__asm
		{
			mov ecx, dword ptr [esi]
			mov ecx, dword ptr [ecx + 0x54]
			test ecx, ecx
			je conclusion // invalid vehicle

			push ecx

			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x64]

			pop ecx
			lea ecx, dword ptr [ecx - 0x4C + 0x758]

			push eax
			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x7C]
			test al, al

			mov ecx, dword ptr withinSightColour
			mov edx, dword ptr outOfSightColour
			cmove ecx, edx // not in LOS

			push ecx
			push dword ptr [ebx + 0xCC]
			call updateFEngColour
			add esp, 0x8

			conclusion:
			// Execute original code and resume
			mov byte ptr [esp + 0x13], 0x1

			jmp dword ptr coneIconExit
		}
	}





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool ParseColour
	(
		HeatParameters::Parser& parser,
		const char* const       colourName,
		uint32_t&               colourCode
	) {
		int red   = 0;
		int green = 0;
		int blue  = 0;
		int alpha = 0;

		const bool isValid = parser.ParseParameterRow<int, int, int, int>
		(
			"Helicopter:Vision",
			colourName,
			{red,   0, 255},
			{green, 0, 255},
			{blue,  0, 255},
			{alpha, 0, 255}
		);

		if (isValid)
			colourCode = (alpha << 24) | (red << 16) | (green << 8) | blue;

		return isValid;
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		if (not ParseColour(parser, "withinSightColour", withinSightColour)) return false;
		if (not ParseColour(parser, "outOfSightColour",  outOfSightColour))  return false;

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

		Globals::logger.LogLongIndent("Within sight:", static_cast<uint32_t>(withinSightColour));
		Globals::logger.LogLongIndent("Out of sight:", static_cast<uint32_t>(outOfSightColour));
	}
}