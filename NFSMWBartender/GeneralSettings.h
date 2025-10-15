#pragma once

#include "Globals.h"
#include "MemoryEditor.h"
#include "HeatParameters.h"



namespace GeneralSettings 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<float> bountyIntervals     (10.f); // seconds
	HeatParameters::Pair<int>   maxBountyMultipliers(3);    // kills

	HeatParameters::Pair<float> maxBustDistances(15.f); // metres
	HeatParameters::Pair<float> evadeTimers     (7.f);  // seconds
	HeatParameters::Pair<float> bustTimers      (5.f);

	HeatParameters::Pair<bool> doCopFlipDamageChecks(true);

	HeatParameters::Pair<bool>  copFlipTimeCheckEnableds(false);
	HeatParameters::Pair<float> copFlipTimeCheckDelays  (6.f);  // seconds

	HeatParameters::Pair<bool>  racerFlipResetEnableds(false);
	HeatParameters::Pair<float> racerFlipResetDelays  (4.f);   // seconds

	// Conversions
	float bountyFrequency = 1.f / bountyIntervals.current; // seconds

	float halfEvadeRate = .5f / evadeTimers.current; // Hertz
	float bustRate      = 1.f / bustTimers.current;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copComboEntrance = 0x418FBA;
	constexpr address copComboExit     = 0x418FD2;

	__declspec(naked) void CopCombo()
	{
		__asm
		{
			mov eax, dword ptr maxBountyMultipliers.current

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



	constexpr address copFlippingEntrance = 0x6B19AE;
	constexpr address copFlippingExit     = 0x6B19CA;

	__declspec(naked) void CopFlipping()
	{
		constexpr static address copFlippingSkip = 0x6B1A0D;

		__asm
		{
			cmp byte ptr copFlipTimeCheckEnableds.current, 0x1
			jne damaged // time check disabled

			fld dword ptr [esi + 0xB8]
			fcomp dword ptr copFlipTimeCheckDelays.current
			fnstsw ax
			test ah, 0x41
			jne damaged // delay has yet to expire

			mov eax, dword ptr [esi + 0x4C]
			lea ecx, dword ptr [esi + 0x4C]
			call dword ptr [eax + 0x1C]
			jmp conclusion // was destroyed

			damaged:
			cmp byte ptr doCopFlipDamageChecks.current, 0x1
			je conclusion // damage check enabled

			jmp dword ptr copFlippingSkip // no flipping enabled

			conclusion:
			jmp dword ptr copFlippingExit
		}
	}



	constexpr address racerFlippingEntrance = 0x6A45A3;
	constexpr address racerFlippingExit     = 0x6A45B1;

	__declspec(naked) void RacerFlipping()
	{
		__asm
		{
			cmp byte ptr racerFlipResetEnableds.current, 0x1
			jne conclusion // flipping resets disabled

			fld dword ptr [esi + 0x54]
			fcomp dword ptr racerFlipResetDelays.current
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr racerFlippingExit
		}
	}



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



	constexpr address maxBustDistanceEntrance = 0x444483;
	constexpr address maxBustDistanceExit     = 0x44448B;

	__declspec(naked) void MaxBustDistance()
	{
		__asm
		{
			fldz
			fcomp dword ptr [esi + 0x168] // EVADE progress
			fnstsw ax
			test ah, 0x1
			jne obstructed                // in EVADE state

			fld dword ptr maxBustDistances.current
			jmp conclusion

			obstructed:
			fldz

			conclusion:
			// Execute original code and resume
			test bl, bl

			jmp dword ptr maxBustDistanceExit
		}
	}
	




    // State management -----------------------------------------------------------------------------------------------------------------------------

    bool Initialise(HeatParameters::Parser& parser)
    {
		if (not parser.LoadFile(HeatParameters::configPathBasic + "General.ini")) return false;

		// Heat parameters
		HeatParameters::Parse<float>(parser, "Bounty:Interval", {bountyIntervals,      .001f});
		HeatParameters::Parse<int>  (parser, "Bounty:Combo",    {maxBountyMultipliers,     1});
		
		HeatParameters::Parse<float, float>(parser, "State:Busting", {bustTimers,  .001f}, {maxBustDistances, 0.f});
		HeatParameters::Parse<float>       (parser, "State:Evading", {evadeTimers, .001f});

		HeatParameters::Parse<bool>         (parser, "Flipping:Damage", {doCopFlipDamageChecks});
		HeatParameters::ParseOptional<float>(parser, "Flipping:Time",   copFlipTimeCheckEnableds, {copFlipTimeCheckDelays, .001f});
		HeatParameters::ParseOptional<float>(parser, "Flipping:Reset",  racerFlipResetEnableds,   {racerFlipResetDelays,   .001f});

		// Code caves
		MemoryEditor::Write<float*>(&bustRate,              {0x40AEDB});
		MemoryEditor::Write<float*>(&halfEvadeRate,         {0x444A3A});
		MemoryEditor::Write<float*>(&(bustTimers.current),  {0x4445CE});
		MemoryEditor::Write<float*>(&bountyFrequency,       {0x444513, 0x444524});
		MemoryEditor::Write<float*>(&(evadeTimers.current), {0x4448E6, 0x444802, 0x4338F8});

		MemoryEditor::DigCodeCave(CopCombo,        copComboEntrance,        copComboExit);
		MemoryEditor::DigCodeCave(CopFlipping,     copFlippingEntrance,     copFlippingExit);
		MemoryEditor::DigCodeCave(RacerFlipping,   racerFlippingEntrance,   racerFlippingExit);
		MemoryEditor::DigCodeCave(PassiveBounty,   passiveBountyEntrance,   passiveBountyExit);
        MemoryEditor::DigCodeCave(MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);

		// Status flag
		featureEnabled = true;

		return true;
    }



    void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		bountyIntervals     .SetToHeat(isRacing, heatLevel);
		maxBountyMultipliers.SetToHeat(isRacing, heatLevel);

		bountyFrequency = 1.f / bountyIntervals.current;

		bustTimers      .SetToHeat(isRacing, heatLevel);
		maxBustDistances.SetToHeat(isRacing, heatLevel);
		evadeTimers     .SetToHeat(isRacing, heatLevel);

		halfEvadeRate = .5f / evadeTimers.current;
		bustRate      = 1.f / bustTimers.current;

		doCopFlipDamageChecks   .SetToHeat(isRacing, heatLevel);
		copFlipTimeCheckEnableds.SetToHeat(isRacing, heatLevel);
		copFlipTimeCheckDelays  .SetToHeat(isRacing, heatLevel);
		racerFlipResetEnableds  .SetToHeat(isRacing, heatLevel);
		racerFlipResetDelays    .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [GEN] GeneralSettings");

			Globals::logger.LogLongIndent("bountyInterval          ", bountyIntervals.current);
			Globals::logger.LogLongIndent("maxBountyMultiplier     ", maxBountyMultipliers.current);

			Globals::logger.LogLongIndent("bustTimer               ", bustTimers.current);
			Globals::logger.LogLongIndent("maxBustDistance         ", maxBustDistances.current);
			Globals::logger.LogLongIndent("evadeTimer              ", evadeTimers.current);

			Globals::logger.LogLongIndent("doCopFlipDamageCheck    ", doCopFlipDamageChecks.current);

			if (copFlipTimeCheckEnableds.current)
				Globals::logger.LogLongIndent("copFlipTimeCheckDelay   ", copFlipTimeCheckDelays.current);

			if (racerFlipResetEnableds.current)
				Globals::logger.LogLongIndent("racerFlipResetDelay     ", racerFlipResetDelays.current);
		}
    }
}