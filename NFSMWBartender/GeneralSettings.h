#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"



namespace GeneralSettings 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool> rivalPursuitsEnableds(true); // flag

	HeatParameters::Pair<float> bountyIntervals     (10.f); // seconds
	HeatParameters::Pair<int>   maxBountyMultipliers(3);    // times

	HeatParameters::Pair<float> maxBustDistances(15.f); // metres
	HeatParameters::Pair<float> evadeTimers     (7.f);  // seconds
	HeatParameters::Pair<float> bustTimers      (5.f);  // seconds

	HeatParameters::Pair<bool> copFlipByDamageEnableds(true); // flag

	HeatParameters::OptionalPair<float> copFlipByTimers;      // seconds
	HeatParameters::OptionalPair<float> racerFlipResetDelays; // seconds

	// Conversions
	float bountyFrequency = 1.f / bountyIntervals.current; // seconds
	float halfEvadeRate   = .5f / evadeTimers.current;     // hertz
	float bustRate        = 1.f / bustTimers.current;      // hertz

	// Code caves
	HashContainers::CachedVaultMap<bool> copTypeToIsUnaffected(false);





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
			cmp byte ptr copFlipByTimers.isEnableds.current, 0x1
			jne damaged // time check disabled

			fld dword ptr [esi + 0xB8]
			fcomp dword ptr copFlipByTimers.values.current
			fnstsw ax
			test ah, 0x41
			jne damaged // delay has yet to expire

			mov eax, dword ptr [esi + 0x4C]
			lea ecx, dword ptr [esi + 0x4C]
			call dword ptr [eax + 0x1C]
			jmp conclusion // was destroyed

			damaged:
			cmp byte ptr copFlipByDamageEnableds.current, 0x1
			jne skip // damage check disabled

			conclusion:
			jmp dword ptr copFlippingExit

			skip:
			jmp dword ptr copFlippingSkip
		}
	}



	constexpr address rivalPursuitEntrance = 0x426C9C;
	constexpr address rivalPursuitExit     = 0x426CA4;

	__declspec(naked) void RivalPursuit()
	{
		__asm
		{
			cmp byte ptr rivalPursuitsEnableds.current, 0x0
			je conclusion // rival pursuits disabled

			// Execute original and resume
			mov ecx, dword ptr [eax + 0x1968]
			test ecx, ecx

			conclusion:
			jmp dword ptr rivalPursuitExit
		}
	}



	constexpr address breakerCheckEntrance = 0x42E967;
	constexpr address breakerCheckExit     = 0x42E96C;

	__declspec(naked) void BreakerCheck()
	{
		__asm
		{
			call dword ptr [eax + 0x7C]
			test al, al
			jne conclusion // vehicle destroyed

			mov ecx, ebx
			call Globals::GetVehicleType

			push eax // copType
			mov ecx, offset copTypeToIsUnaffected
			call HashContainers::CachedVaultMap<bool>::GetValue
			test al, al

			conclusion:
			jmp dword ptr breakerCheckExit
		}
	}



	constexpr address racerFlippingEntrance = 0x6A45A3;
	constexpr address racerFlippingExit     = 0x6A45B1;

	__declspec(naked) void RacerFlipping()
	{
		__asm
		{
			cmp byte ptr racerFlipResetDelays.isEnableds.current, 0x1
			jne conclusion // flipping resets disabled

			fld dword ptr [esi + 0x54]
			fcomp dword ptr racerFlipResetDelays.values.current
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
		// Also fixes getting (hidden) BUSTED progress while the green EVADE bar fills
		MemoryTools::MakeRangeJMP(MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);
	}



    bool Initialise(HeatParameters::Parser& parser)
    {
		if (not parser.LoadFile(HeatParameters::configPathBasic + "General.ini")) return false;

		// Heat parameters
		HeatParameters::Parse<bool>(parser, "Pursuits:Rivals", {rivalPursuitsEnableds});

		HeatParameters::Parse<float>(parser, "Bounty:Interval", {bountyIntervals,      .001f});
		HeatParameters::Parse<int>  (parser, "Bounty:Combo",    {maxBountyMultipliers,     1});
		
		HeatParameters::Parse<float, float>(parser, "State:Busting", {bustTimers,  .001f}, {maxBustDistances, 0.f});
		HeatParameters::Parse<float>       (parser, "State:Evading", {evadeTimers, .001f});

		HeatParameters::Parse        <bool> (parser, "Flipping:Damage", {copFlipByDamageEnableds});
		HeatParameters::ParseOptional<float>(parser, "Flipping:Time",   {copFlipByTimers,      0.f});
		HeatParameters::ParseOptional<float>(parser, "Flipping:Reset",  {racerFlipResetDelays, 0.f});

		// Breaker flags
		std::vector<std::string> copVehicles;
		std::vector<bool>        isAffecteds;

		const size_t numCopVehicles = parser.ParseUserParameter("Vehicles:Breakers", copVehicles, isAffecteds);

		if (numCopVehicles > 0)
		{
			for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
				copTypeToIsUnaffected.try_emplace(Globals::GetVaultKey(copVehicles[vehicleID].c_str()), (not isAffecteds[vehicleID]));

			// Code modifications (feature-specific)
			MemoryTools::MakeRangeJMP(BreakerCheck, breakerCheckEntrance, breakerCheckExit);
		}

		// Code modifications (general)
		MemoryTools::Write<float*>(&bustRate,              {0x40AEDB});
		MemoryTools::Write<float*>(&halfEvadeRate,         {0x444A3A});
		MemoryTools::Write<float*>(&(bustTimers.current),  {0x4445CE});
		MemoryTools::Write<float*>(&bountyFrequency,       {0x444513, 0x444524});
		MemoryTools::Write<float*>(&(evadeTimers.current), {0x4448E6, 0x444802, 0x4338F8});

		MemoryTools::MakeRangeJMP(CopCombo,      copComboEntrance,      copComboExit);
		MemoryTools::MakeRangeJMP(CopFlipping,   copFlippingEntrance,   copFlippingExit);
		MemoryTools::MakeRangeJMP(RivalPursuit,  rivalPursuitEntrance,  rivalPursuitExit);
		MemoryTools::MakeRangeJMP(RacerFlipping, racerFlippingEntrance, racerFlippingExit);
		MemoryTools::MakeRangeJMP(PassiveBounty, passiveBountyEntrance, passiveBountyExit);

		ApplyFixes(); // also contains bust-distance feature

		// Status flag
		featureEnabled = true;

		return true;
    }



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [GEN] GeneralSettings");

		rivalPursuitsEnableds.Log("rivalPursuitsEnabled    ");

		bountyIntervals     .Log("bountyInterval          ");
		maxBountyMultipliers.Log("maxBountyMultiplier     ");

		bustTimers      .Log("bustTimer               ");
		maxBustDistances.Log("maxBustDistance         ");
		evadeTimers     .Log("evadeTimer              ");

		copFlipByDamageEnableds.Log("copFlipByDamageEnabled  ");
		copFlipByTimers        .Log("copFlipByTimer          ");
		racerFlipResetDelays   .Log("racerFlipResetDelay     ");
	}



	void Validate()
	{
		if (copTypeToIsUnaffected.empty()) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [GEN] GeneralSettings");

		copTypeToIsUnaffected.Validate
		(
			"Vehicle-to-unbreakability",
			[=](const vault   key) {return Globals::VehicleClassMatches(key, Globals::Class::ANY);},
			[=](const bool  value) {return true;}
		);
	}



    void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		rivalPursuitsEnableds.SetToHeat(isRacing, heatLevel);

		bountyIntervals     .SetToHeat(isRacing, heatLevel);
		maxBountyMultipliers.SetToHeat(isRacing, heatLevel);

		bountyFrequency = 1.f / bountyIntervals.current;

		bustTimers      .SetToHeat(isRacing, heatLevel);
		maxBustDistances.SetToHeat(isRacing, heatLevel);
		evadeTimers     .SetToHeat(isRacing, heatLevel);

		halfEvadeRate = .5f / evadeTimers.current;
		bustRate      = 1.f / bustTimers.current;

		copFlipByDamageEnableds.SetToHeat(isRacing, heatLevel);
		copFlipByTimers        .SetToHeat(isRacing, heatLevel);
		racerFlipResetDelays   .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
    }
}