#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace GameAdjustments
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address helicopterIconEntrance = 0x579EE5;
	constexpr address helicopterIconExit     = 0x579EEA;

	__declspec(naked) void HelicopterIcon()
	{
		static constexpr address helicopterIconSkip = 0x57A09F;

		__asm
		{
			je conclusion // is helicopter

			cmp dword ptr [esp + 0x18], 0x8
			jge skip // at vehicle cap

			conclusion:
			// Execute original code and resume
			mov eax, 0x91CF00
			mov al, byte ptr [eax]

			jmp dword ptr helicopterIconExit

			skip:
			jmp dword ptr helicopterIconSkip
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
        // Helicopter shadow
        MemoryTools::Write<float>(0.f, {0x903660});

        // Disappearing helicopter mini-map icon
        MemoryTools::ClearAddressRange(0x579EA2, 0x579EAB);
		MemoryTools::DigCodeCave      (HelicopterIcon, helicopterIconEntrance, helicopterIconExit);

        // Heat-level reset (credit: ExOptsTeam)
        MemoryTools::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
        MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});

		// Status flag
		featureEnabled = true;

		return true;
	}
}