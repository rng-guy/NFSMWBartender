#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace GeneralSettings 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<float> bountyIntervals     (10.f); // seconds
	HeatParameters::Pair<int>   maxBountyMultipliers(3);

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
	float halfEvadeRate   = .5f / evadeTimers.current;     // hertz
	float bustRate        = 1.f / bustTimers.current;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copComboEntrance = 0x418FBA;
	constexpr address copComboExit     = 0x418FD2;

	__declspec(naked) void CopCombo()
	{
		__asm
		{
			mov ecx, dword ptr maxBountyMultipliers.current
			mov eax, dword ptr [esi + 0xF0]

			inc eax
			cmp eax, ecx
			cmovl ecx, eax // count below maximum

			mov dword ptr [esi + 0xF0], ecx

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
			jne skip // damage check disabled

			conclusion:
			jmp dword ptr copFlippingExit

			skip:
			jmp dword ptr copFlippingSkip
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
			sub eax, ebx   // accounts for short intervals
			jle conclusion // below threshold

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
			mov ecx, 0x890968 // pointer -> 0.f
			mov edx, offset maxBustDistances.current

			fldz
			fcomp dword ptr [esi + 0x168] // EVADE state
			fnstsw ax
			test ah, 0x1
			cmovne edx, ecx               // LOS broken

			// Execute original code and resume
			test bl, bl
			fld dword ptr [edx]

			jmp dword ptr maxBustDistanceExit
		}
	}
	




    // State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		static bool fixesApplied = false;

		if (fixesApplied) return;

		// Also fixes getting (hidden) BUSTED progress while the green EVADE bar fills
		MemoryTools::MakeRangeJMP(MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);

		fixesApplied = true;
	}



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

		// Code modifications 
		MemoryTools::Write<float*>(&bustRate,              {0x40AEDB});
		MemoryTools::Write<float*>(&halfEvadeRate,         {0x444A3A});
		MemoryTools::Write<float*>(&(bustTimers.current),  {0x4445CE});
		MemoryTools::Write<float*>(&bountyFrequency,       {0x444513, 0x444524});
		MemoryTools::Write<float*>(&(evadeTimers.current), {0x4448E6, 0x444802, 0x4338F8});

		MemoryTools::MakeRangeJMP(CopCombo,      copComboEntrance,      copComboExit);
		MemoryTools::MakeRangeJMP(CopFlipping,   copFlippingEntrance,   copFlippingExit);
		MemoryTools::MakeRangeJMP(RacerFlipping, racerFlippingEntrance, racerFlippingExit);
		MemoryTools::MakeRangeJMP(PassiveBounty, passiveBountyEntrance, passiveBountyExit);

		ApplyFixes();

		// Status flag
		featureEnabled = true;

		return true;
    }



	void LogHeatReport()
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
			LogHeatReport();
    }
}