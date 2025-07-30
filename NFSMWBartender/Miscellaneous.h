#pragma once

#include <Windows.h>
#include <array>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace Miscellaneous 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	float bountyInterval = 10.f; // seconds
	int   copComboLimit  = 3;    // kills

	// Free-roam Heat levels
	std::array<float, Globals::maxHeatLevel> roamBountyIntervals = {};
	std::array<int,   Globals::maxHeatLevel> roamCopComboLimits  = {};

	// Racing Heat levels
	std::array<float, Globals::maxHeatLevel> raceBountyIntervals = {};
	std::array<int,   Globals::maxHeatLevel> raceCopComboLimits = {};

	// Code caves
	constexpr address floorFunction = 0x7C4B80;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address passiveBountyEntrance = 0x44450D;
	constexpr address passiveBountyExit     = 0x444542;

	__declspec(naked) void PassiveBounty()
	{
		__asm
		{
			fld dword ptr [esp + 0x30] // previous timestamp
			fdiv dword ptr bountyInterval
			call dword ptr floorFunction // pops st(0)
			mov ebx, eax

			fld dword ptr [esi + 0xF4] // current
			fdiv dword ptr bountyInterval
			call dword ptr floorFunction

			cmp eax, ebx
			je conclusion // not yet at interval threshold

			sub eax, ebx // accounts for short intervals
			imul eax, dword ptr [esi + 0x174]

			mov edx, dword ptr [ebp]
			push eax
			mov ecx, ebp
			call dword ptr [edx + 0x3C]

			conclusion:
			jmp dword ptr passiveBountyExit
		}
	}



	constexpr address copComboEntrance = 0x418FBA;
	constexpr address copComboExit     = 0x418FD2;

	__declspec(naked) void CopCombo()
	{
		__asm
		{
			mov eax, dword ptr copComboLimit

			cmp dword ptr [esi + 0xF0], eax
			je conclusion // at current limit
			jg reset      // above

			mov eax, dword ptr [esi + 0xF0]
			inc eax

			reset:
			mov dword ptr [esi + 0xF0], eax

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esi + 0xF0]

			jmp dword ptr copComboExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    bool Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configPathBasic + "Others.ini")) return false;

		// Pursuit parameters
		Globals::ParseHeatParameter<float>
		(
			parser,
			"BountyInterval",
			roamBountyIntervals,
			raceBountyIntervals,
			bountyInterval,
			.001f
		);

		Globals::ParseHeatParameter<int>
		(
			parser,
			"CopComboLimit",
			roamCopComboLimits,
			raceCopComboLimits,
			copComboLimit,
			1
		);

		// Code caves
        MemoryEditor::DigCodeCave(&PassiveBounty, passiveBountyEntrance, passiveBountyExit);
        MemoryEditor::DigCodeCave(&CopCombo,      copComboEntrance,      copComboExit);

        return (featureEnabled = true);
    }



	void SetToHeat
	(
		const size_t heatLevel,
		const bool   isRacing
	) {
        if (not featureEnabled) return;

		if (isRacing)
		{
			bountyInterval = raceBountyIntervals[heatLevel - 1];
			copComboLimit  = raceCopComboLimits[heatLevel - 1];
		}
		else
		{
			bountyInterval = roamBountyIntervals[heatLevel - 1];
			copComboLimit  = roamCopComboLimits[heatLevel - 1];
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[MSC] Miscellaneous");

			Globals::LogLongIndent("bountyInterval         :", bountyInterval);
			Globals::LogLongIndent("copComboLimit          :", copComboLimit);
		}
    }
}