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

	// General Heat levels
	std::array<float, Globals::maxHeatLevel> bountyIntervals = {};
	std::array<int,   Globals::maxHeatLevel> copComboLimits  = {};

	// Code caves
	const address floorFunction = 0x7C4B80;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address passiveBountyEntrance = 0x44450D;
	const address passiveBountyExit     = 0x444542;

	__declspec(naked) void PassiveBounty()
	{
		__asm
		{
			fld dword ptr [esp + 0x30] // previous timestamp
			fdiv dword ptr bountyInterval
			call dword ptr floorFunction // pops st(0)
			mov ebx, eax

			fld dword ptr[esi + 0xF4] // current
			fdiv dword ptr bountyInterval
			call dword ptr floorFunction

			cmp eax, ebx
			je conclusion // not yet at interval threshold

			sub eax, ebx // accounts for short intervals
			imul eax, [esi + 0x174]

			mov edx, [ebp]
			push eax
			mov ecx, ebp
			call dword ptr [edx + 0x3C]

			conclusion:
			jmp dword ptr passiveBountyExit
		}
	}



	const address copComboEntrance = 0x418FBA;
	const address copComboExit     = 0x418FD2;

	__declspec(naked) void CopCombo()
	{
		__asm
		{
			mov eax, copComboLimit

			cmp [esi + 0xF0], eax
			je conclusion // at current limit
			jg reset      // above

			mov eax, [esi + 0xF0]
			inc eax

			reset:
			mov [esi + 0xF0], eax

			conclusion:
			// Execute original code and resume
			mov ecx, [esi + 0xF0]

			jmp dword ptr copComboExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    void Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configBasicPath + "Others.ini")) return;

        parser.ParseFormatParameter<float>("BountyInterval", Globals::configFormat, bountyIntervals, bountyInterval, .001f);
        parser.ParseFormatParameter<int>  ("CopComboLimit",  Globals::configFormat, copComboLimits,  copComboLimit,  1);

        MemoryEditor::DigCodeCave(&PassiveBounty, passiveBountyEntrance, passiveBountyExit);
        MemoryEditor::DigCodeCave(&CopCombo,      copComboEntrance,      copComboExit);

        featureEnabled = true;
    }



    void SetToHeat(const size_t heatLevel)
    {
        if (not featureEnabled) return;

        bountyInterval = bountyIntervals[heatLevel - 1];
        copComboLimit  = copComboLimits[heatLevel - 1];
    }
}