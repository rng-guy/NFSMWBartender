#pragma once

#include <string>
#include <algorithm>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"



namespace GeneralSettings 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Pursuit behaviour
	bool trackPursuitLength  = false;
	bool trackUnitsInPursuit = false;
	bool trackCopsLost       = false;
	bool trackCopsHit        = false;
	bool trackCopsDestroyed  = false;
	bool trackPassiveBounty  = false;
	bool trackPropertyDamage = false;
	bool trackInfractions    = false;

	// Heat parameters
	HeatParameters::Pair<bool> rivalPursuitsEnableds(true);

	HeatParameters::Pair<float> bountyIntervals     (10.f, {.001f}); // seconds
	HeatParameters::Pair<int>   maxBountyMultipliers(3,    {1});     // scale

	HeatParameters::Pair<float> bustTimers      (5.f,  {.001f}); // seconds
	HeatParameters::Pair<float> maxBustDistances(15.f, {0.f});   // metres
	HeatParameters::Pair<float> evadeTimers     (7.f,  {.001f}); // seconds

	HeatParameters::Pair<bool> carsAffectedByHidings (true);
	HeatParameters::Pair<bool> helisAffectedByHidings(true);

	HeatParameters::Pair<bool> copFlipByDamageEnableds(true);

	HeatParameters::OptionalPair<float> copFlipByTimers;      // seconds
	HeatParameters::OptionalPair<float> racerFlipResetDelays; // seconds

	// Conversions
	float bountyFrequency = 1.f / bountyIntervals.current; // hertz
	float halfEvadeRate   = .5f / evadeTimers.current;     // hertz
	float bustRate        = 1.f / bustTimers.current;      // hertz

	// Code caves
	HashContainers::CachedCopyVaultMap<bool> copTypeToIsBreakerImmune(false);


	


	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copComboEntrance = 0x418FBA;
	constexpr address copComboExit     = 0x418FD2;

	// Updates the combo-bounty counter
	__declspec(naked) void CopCombo()
	{
		__asm
		{
			mov ecx, dword ptr maxBountyMultipliers.current
			mov eax, dword ptr [esi + 0xF0] // combo count

			inc eax
			cmp eax, ecx
			cmovl ecx, eax // count below maximum

			mov dword ptr [esi + 0xF0], ecx

			jmp dword ptr copComboExit
		}
	}



	constexpr address heatUpdateEntrance = 0x443DFD;
	constexpr address heatUpdateExit     = 0x443E16;

	// The code after this forces unconditional bounty-gain updates after races
	__declspec(naked) void HeatUpdate()
	{
		__asm
		{
			je conclusion // Heat level unchanged

			xor edx, edx

			mov dword ptr [esi + 0x104], edx // formation duration
			mov dword ptr [esi + 0xD8], eax  // current Heat level
			mov byte ptr [esi + 0x254], dl   // Cross priority flag

			conclusion:
			jmp dword ptr heatUpdateExit
		}
	}



	constexpr address copFlippingEntrance = 0x6B19AE;
	constexpr address copFlippingExit     = 0x6B19CA;

	// Checks whether flipped cop vehicles should be destroyed
	__declspec(naked) void CopFlipping()
	{
		constexpr static address copFlippingSkip = 0x6B1A0D;

		__asm
		{
			cmp byte ptr copFlipByTimers.isEnableds.current, 0x1
			jne damaged // time check disabled

			fld dword ptr [esi + 0xB8] // time spent flipped
			fcomp dword ptr copFlipByTimers.values.current
			fnstsw ax
			test ah, 0x41
			jne damaged                // delay has yet to expire

			mov eax, dword ptr [esi + 0x4C]
			lea ecx, dword ptr [esi + 0x4C]
			call dword ptr [eax + 0x1C] // DamageVehicle::Destroy
			jmp conclusion              // cop was destroyed

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

	// Decides whether cops can start non-player pursuits
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



	constexpr address breakerCheckEntrance = 0x42E963;
	constexpr address breakerCheckExit     = 0x42E96C;

	// Decides whether cops are affected by any active pursuit breakers
	__declspec(naked) void BreakerCheck()
	{
		__asm
		{
			// Execute original code first
			mov ecx, ebx
			call Globals::IsVehicleDestroyed
			test al, al
			jne conclusion // vehicle destroyed

			mov ecx, ebx
			call Globals::GetVehicleType

			push eax // copType
			mov ecx, offset copTypeToIsBreakerImmune
			call HashContainers::CachedCopyVaultMap<bool>::GetValue
			test al, al

			conclusion:
			jmp dword ptr breakerCheckExit
		}
	}



	constexpr address racerFlippingEntrance = 0x6A45A3;
	constexpr address racerFlippingExit     = 0x6A45B1;

	// Checks whether flipped racers should be reset
	__declspec(naked) void RacerFlipping()
	{
		__asm
		{
			cmp byte ptr racerFlipResetDelays.isEnableds.current, 0x1
			jne conclusion // flipping resets disabled

			fld dword ptr [esi + 0x54] // time spent flipped
			fcomp dword ptr racerFlipResetDelays.values.current
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr racerFlippingExit
		}
	}



	constexpr address passiveBountyEntrance = 0x44452F;
	constexpr address passiveBountyExit     = 0x444542;

	// Checks whether passive bounty is due
	__declspec(naked) void PassiveBounty()
	{
		__asm
		{
			sub eax, ebx
			jle conclusion // interval incomplete

			imul eax, dword ptr [esi + 0x174] // bounty increment

			mov edx, dword ptr [ebp]
			push eax
			mov ecx, ebp
			call dword ptr [edx + 0x3C] // AIPerpVehicle::AddToPendingRepPointsNormal

			conclusion:
			jmp dword ptr passiveBountyExit
		}
	}



	constexpr address maxBustDistanceEntrance = 0x444483;
	constexpr address maxBustDistanceExit     = 0x44448B;

	// Decides at which distance from cops racers can be busted
	__declspec(naked) void MaxBustDistance()
	{
		__asm
		{
			fldz
			fcom dword ptr [esi + 0x168] // "EVADE" state
			fnstsw ax
			test ah, 0x5
			jne conclusion               // evading

			fstp st(0)
			fld dword ptr maxBustDistances.current
			
			// Execute original code and resume
			conclusion:
			test bl, bl

			jmp dword ptr maxBustDistanceExit
		}
	}
	


	constexpr address heatEscalationEntrance = 0x443D93;
	constexpr address heatEscalationExit     = 0x443D9B;

	// Corrects the VltEd array index based on Blacklist rank
	__declspec(naked) void HeatEscalation()
	{
		__asm
		{
			xor edx, edx

			dec eax
			cmovl eax, edx // Blacklist index negative

			jmp dword ptr heatEscalationExit
		}
	}



	constexpr address destructionBountyEntrance = 0x418F5B;
	constexpr address destructionBountyExit     = 0x418F61;

	// Corrects the VltEd array index based on Heat level
	__declspec(naked) void DestructionBounty()
	{
		__asm
		{
			mov edi, dword ptr [esi + 0x98] // current Heat level
			xor ecx, ecx

			dec edi
			cmovl edi, ecx // Heat index negative

			jmp dword ptr destructionBountyExit
		}
	}



	constexpr address hiddenFromCarsEntrance = 0x416571;
	constexpr address hiddenFromCarsExit     = 0x41657A;

	// Decides whether racers are invisible to cop cars
	__declspec(naked) void HiddenFromCars()
	{
		__asm
		{
			mov al, byte ptr carsAffectedByHidings.current
			test al, byte ptr [edi + 0x2C] // hidden from cars

			jmp dword ptr hiddenFromCarsExit
		}
	}



	constexpr address hiddenFromRoadblocksEntrance = 0x444329;
	constexpr address hiddenFromRoadblocksExit     = 0x444333;

	// Decides whether racers are invisible to roadblocks
	__declspec(naked) void HiddenFromRoadblocks()
	{
		__asm
		{
			mov al, byte ptr carsAffectedByHidings.current
			test al, byte ptr [ebp + 0x2C] // hidden from cars

			jmp dword ptr hiddenFromRoadblocksExit
		}
	}



	constexpr address hiddenFromHelicoptersEntrance = 0x417103;
	constexpr address hiddenFromHelicoptersExit     = 0x41710C;

	// Decides whether racers are invisible to helicopters
	__declspec(naked) void HiddenFromHelicopters()
	{
		__asm
		{
			mov al, byte ptr helisAffectedByHidings.current
			test al, byte ptr [esi + 0x2D] // hidden from helicopters

			jmp dword ptr hiddenFromHelicoptersExit
		}
	}





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ParseTracking
	(
		HeatParameters::Parser& parser,
		const std::string&      trackingName,
		bool&                   isTracked,
		const address           rangeStart,
		const address           rangeEnd
	) {
		parser.ParseFromFile<bool>("Pursuits:Races", trackingName, {isTracked});

		if (isTracked)
		{
			MemoryTools::MakeRangeNOP(rangeStart, rangeEnd);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Tracking", trackingName);
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Fixes update for passive bounty amount after race pursuits
		MemoryTools::MakeRangeJMP(HeatUpdate, heatUpdateEntrance, heatUpdateExit);

		// Also fixes getting (hidden) BUSTED progress while the green EVADE bar fills
		MemoryTools::MakeRangeJMP(MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);

		// Fixes incorrect array values read from VltEd
		MemoryTools::MakeRangeJMP(HeatEscalation,    heatEscalationEntrance,    heatEscalationExit);
		MemoryTools::MakeRangeJMP(DestructionBounty, destructionBountyEntrance, destructionBountyExit);
	}



    bool Initialise(HeatParameters::Parser& parser)
    {
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [GEN] GeneralSettings");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "General.ini")) return false;

		// Pursuit tracking (and code modifications)
		ParseTracking(parser, "pursuitLength",  trackPursuitLength,  0x443CBE, 0x443CC8);
		ParseTracking(parser, "unitsInPursuit", trackUnitsInPursuit, 0x41911B, 0x419125);
		ParseTracking(parser, "copsLost",       trackCopsLost,       0x42B761, 0x42B76B);
		ParseTracking(parser, "copsHit",        trackCopsHit,        0x40AF43, 0x40AF4D);
		ParseTracking(parser, "copsDestroyed",  trackCopsDestroyed,  0x418F33, 0x418F41);
		ParseTracking(parser, "passiveBounty",  trackPassiveBounty,  0x4094A0, 0x4094AA);
		ParseTracking(parser, "propertyDamage", trackPropertyDamage, 0x409463, 0x409467);
		ParseTracking(parser, "infractions",    trackInfractions,    0x5FDDDC, 0x5FDDE7);
		
		if (trackCopsDestroyed)
		{
			MemoryTools::MakeRangeNOP(0x4094E0, 0x4094EA); // cop bounty
			MemoryTools::MakeRangeNOP(0x43EA15, 0x43EA19); // total cops destroyed
		}

		// Heat parameters
		HeatParameters::Parse(parser, "Pursuits:Rivals", rivalPursuitsEnableds);

		HeatParameters::Parse(parser, "Bounty:Interval", bountyIntervals);
		HeatParameters::Parse(parser, "Bounty:Combo",    maxBountyMultipliers);

		HeatParameters::Parse(parser, "State:Busting",  bustTimers, maxBustDistances);
		HeatParameters::Parse(parser, "State:Evading",  evadeTimers);
		HeatParameters::Parse(parser, "Evading:Hiding", carsAffectedByHidings, helisAffectedByHidings);

		HeatParameters::Parse(parser, "Flipping:Damaged", copFlipByDamageEnableds);
		HeatParameters::Parse(parser, "Flipping:Time",    copFlipByTimers);
		HeatParameters::Parse(parser, "Flipping:Reset",   racerFlipResetDelays);

		// Breaker flags
		std::vector<std::string> copVehicles;
		std::vector<bool>        isAffecteds;

		parser.ParseUser<bool>("Vehicles:Breakers", copVehicles, {isAffecteds});

		// Populate breaker-immunity map
		const bool mapIsValid = copTypeToIsBreakerImmune.FillFromVectors<std::string, bool>
		(
			"Vehicle-to-immunity",
			HeatParameters::defaultValueHandle,
			copVehicles,
			Globals::StringToVaultKey,
			Globals::DoesVehicleExist,
			isAffecteds,
			[](const bool isAffected) -> bool {return (not isAffected);},
			[](const bool isImmune)   -> bool {return true;}
		);

		if (mapIsValid)
		{
			// Code modifications (feature-specific)
			MemoryTools::MakeRangeJMP(BreakerCheck, breakerCheckEntrance, breakerCheckExit);
		}

		// Code modifications (general)
		MemoryTools::Write<float*>(&bustRate,              {0x40AEDB});
		MemoryTools::Write<float*>(&halfEvadeRate,         {0x444A3A});
		MemoryTools::Write<float*>(&(bustTimers.current),  {0x4445CE});
		MemoryTools::Write<float*>(&bountyFrequency,       {0x444513, 0x444524});
		MemoryTools::Write<float*>(&(evadeTimers.current), {0x4448E6, 0x444802, 0x4338F8});

		MemoryTools::MakeRangeJMP(CopCombo,              copComboEntrance,              copComboExit);
		MemoryTools::MakeRangeJMP(CopFlipping,           copFlippingEntrance,           copFlippingExit);
		MemoryTools::MakeRangeJMP(RivalPursuit,          rivalPursuitEntrance,          rivalPursuitExit);
		MemoryTools::MakeRangeJMP(RacerFlipping,         racerFlippingEntrance,         racerFlippingExit);
		MemoryTools::MakeRangeJMP(PassiveBounty,         passiveBountyEntrance,         passiveBountyExit);
		MemoryTools::MakeRangeJMP(HiddenFromCars,        hiddenFromCarsEntrance,        hiddenFromCarsExit);
		MemoryTools::MakeRangeJMP(HiddenFromRoadblocks,  hiddenFromRoadblocksEntrance,  hiddenFromRoadblocksExit);
		MemoryTools::MakeRangeJMP(HiddenFromHelicopters, hiddenFromHelicoptersEntrance, hiddenFromHelicoptersExit);

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

		carsAffectedByHidings .Log("carsAffectedByHiding    ");
		helisAffectedByHidings.Log("helisAffectedByHiding   ");

		copFlipByDamageEnableds.Log("copFlipByDamageEnabled  ");
		copFlipByTimers        .Log("copFlipByTimer          ");
		racerFlipResetDelays   .Log("racerFlipResetDelay     ");
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

		carsAffectedByHidings .SetToHeat(isRacing, heatLevel);
		helisAffectedByHidings.SetToHeat(isRacing, heatLevel);

		copFlipByDamageEnableds.SetToHeat(isRacing, heatLevel);
		copFlipByTimers        .SetToHeat(isRacing, heatLevel);
		racerFlipResetDelays   .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
    }
}