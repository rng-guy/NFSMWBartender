#pragma once

#include <Windows.h>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace Miscellaneous 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	float bountyInterval  = 10.f; // seconds
	int   copComboLimit   = 3;    // kills

	// Heat levels
	Globals::HeatParametersPair<float> bountyIntervals;
	Globals::HeatParametersPair<int>   copComboLimits;

	// Conversions
	float bountyFrequency = 1.f / bountyInterval;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address passiveBountyEntrance = 0x44452F;
	constexpr address passiveBountyExit     = 0x444542;

	__declspec(naked) void PassiveBounty()
	{
		__asm
		{
			sub eax, ebx  // accounts for short intervals
			je conclusion // not yet at interval threshold

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
		Globals::ParseHeatParameters<float>
		(
			parser,
			"BountyInterval",
			{bountyIntervals, bountyInterval, .001f}
		);

		Globals::ParseHeatParameters<int>
		(
			parser,
			"CopComboLimit",
			{copComboLimits, copComboLimit, 1}
		);

		// Code caves
		MemoryEditor::Write<float*>(&bountyFrequency, 0x444513, 0x444524);

        MemoryEditor::DigCodeCave(PassiveBounty, passiveBountyEntrance, passiveBountyExit);
        MemoryEditor::DigCodeCave(CopCombo,      copComboEntrance,      copComboExit);

        return (featureEnabled = true);
    }



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		bountyInterval = bountyIntervals(isRacing, heatLevel);
		copComboLimit  = copComboLimits (isRacing, heatLevel);

		bountyFrequency = 1.f / bountyInterval;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[MSC] Miscellaneous");

			Globals::LogLongIndent("bountyInterval         :", bountyInterval);
			Globals::LogLongIndent("copComboLimit          :", copComboLimit);
		}
    }
}