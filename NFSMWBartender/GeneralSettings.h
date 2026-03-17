#pragma once

#include <array>
#include <vector>
#include <algorithm>
#include <functional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
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
	constinit HeatParameters::Pair<bool> rivalPursuitsEnableds(true);

	constinit HeatParameters::Pair<float> bountyIntervals     (10.f, {.001f}); // seconds
	constinit HeatParameters::Pair<int>   maxBountyMultipliers(3,    {1});     // scale

	constinit HeatParameters::Pair<float> bustTimers      (5.f,  {.001f}); // seconds
	constinit HeatParameters::Pair<float> maxBustDistances(15.f, {0.f});   // metres

	constinit HeatParameters::Pair<float> evadeTimers(7.f, {.001f}); // seconds

	constinit HeatParameters::Pair<bool> carsAffectedByHidings (true);
	constinit HeatParameters::Pair<bool> helisAffectedByHidings(true);

	constinit HeatParameters::Pair<bool> copFlipByDamageEnableds(true);

	constinit HeatParameters::OptionalPair<float> copFlipByTimers     ({0.f}); // seconds
	constinit HeatParameters::OptionalPair<float> racerFlipResetDelays({0.f}); // seconds

	// Conversions
	float bountyFrequency = 1.f / bountyIntervals.current; // hertz
	
	float bustRate          = 1.f / bustTimers.current; // hertz
	float resetBustScale    = std::max<float>(bustTimers.current / 1.25f, 4.f);
	float recoveryBustDelta = -.25f * std::max<float>(bustTimers.current / 2.5f, 2.f);

	float halfEvadeRate = .5f / evadeTimers.current; // hertz

	// Code caves
	constinit ModContainers::DefaultCopyVaultMap<bool> copTypeToIsBreakerImmune(false);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	const char* __fastcall GetRandomArrestScene(const size_t heatLevel)
	{
		static constexpr std::array<const char*, 8> scenesDefault =
		{
			"ArrestM06",  "ArrestM19",
			"ArrestF06",  "ArrestF07",
			"ArrestM06b", "ArrestM19b",
			"ArrestF06b", "ArrestF07b"
		};

		static constexpr std::array<const char*, 8> scenesHeat1 =
		{
			"ArrestM01",  "ArrestM16",
			"ArrestF02",  "ArrestF18",
			"ArrestM01b", "ArrestM16b",
			"ArrestF02b", "ArrestF18b"
		};

		static constexpr std::array<const char*, 4> scenesHeat2 =
		{
			"ArrestM04",  "ArrestF23",
			"ArrestM04b", "ArrestF23b"
		};

		static constexpr std::array<const char*, 6> scenesHeat3 =
		{
			"ArrestM07",  "ArrestM14",
			"ArrestF14",  "ArrestM07b",
			"ArrestM14b", "ArrestF14b"
		};

		auto   candidates    = scenesDefault.data();
		size_t numCandidates = scenesDefault.size();

		switch (heatLevel)
		{
		case 0:
		case 1:
			candidates    = scenesHeat1.data();
			numCandidates = scenesHeat1.size();
			break;

		case 2:
			candidates    = scenesHeat2.data();
			numCandidates = scenesHeat2.size();
			break;

		case 3:
			candidates    = scenesHeat3.data();
			numCandidates = scenesHeat3.size();
		}

		const auto scene = candidates[Globals::prng.GenerateIndex(numCandidates)];

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<1>("[GEN] Arrest cutscene:", scene);

		return scene;
	}





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

	// Forces unconditional bounty-gain updates after races
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
		static constexpr address copFlippingSkip = 0x6B1A0D;

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



	constexpr address arrestSceneEntrance = 0x44D7A9;
	constexpr address arrestSceneExit     = 0x44D967;

	// Selects a random arrest scene
	__declspec(naked) void ArrestScene()
	{
		__asm
		{
			// Execute original code first
			mov esi, dword ptr [esp + 0x18]
			push dword ptr [esp + 0x1C]

			mov ecx, ebx
			call GetRandomArrestScene // ecx: heatLevel
			push eax

			jmp dword ptr arrestSceneExit
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



	constexpr address pursuitBreakerCheckEntrance = 0x42E963;
	constexpr address pursuitBreakerCheckExit     = 0x42E96C;

	// Decides whether cops are affected by any active pursuit breakers
	__declspec(naked) void PursuitBreakerCheck()
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
			call ModContainers::DefaultCopyVaultMap<bool>::GetValue
			test al, al

			conclusion:
			jmp dword ptr pursuitBreakerCheckExit
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





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseTracking
	(
		const HeatParameters::Parser& parser,
		const std::string_view        key,
		bool&                         isTracked
	) {
		parser.ParseFromFile<bool>("Pursuits:Races", key, {isTracked});

		if constexpr (Globals::loggingEnabled)
		{
			if (isTracked)
				Globals::logger.Log<2>("Tracking", key);
		}

		return isTracked;
	}



	void ParseTrackingSettings(const HeatParameters::Parser& parser)
	{
		if (ParseTracking(parser, "pursuitLength", trackPursuitLength))
			MemoryTools::MakeRangeNOP<0x443CBE, 0x443CC8>();

		if (ParseTracking(parser, "unitsInPursuit", trackUnitsInPursuit))
			MemoryTools::MakeRangeNOP<0x41911B, 0x419125>();

		if (ParseTracking(parser, "copsLost", trackCopsLost))
			MemoryTools::MakeRangeNOP<0x42B761, 0x42B76B>();

		if (ParseTracking(parser, "copsHit", trackCopsHit))
			MemoryTools::MakeRangeNOP<0x40AF43, 0x40AF4D>();

		if (ParseTracking(parser, "copsDestroyed", trackCopsDestroyed))
		{
			MemoryTools::MakeRangeNOP<0x4094E0, 0x4094EA>(); // cop bounty
			MemoryTools::MakeRangeNOP<0x418F33, 0x418F41>(); // cops destroyed
			MemoryTools::MakeRangeNOP<0x43EA15, 0x43EA19>(); // total cops destroyed
		}

		if (ParseTracking(parser, "passiveBounty", trackPassiveBounty))
			MemoryTools::MakeRangeNOP<0x4094A0, 0x4094AA>();

		if (ParseTracking(parser, "propertyDamage", trackPropertyDamage))
			MemoryTools::MakeRangeNOP<0x409463, 0x409467>();

		if (ParseTracking(parser, "infractions", trackInfractions))
			MemoryTools::MakeRangeNOP<0x5FDDDC, 0x5FDDE7>();
	}



	bool ParsePursuitBreakerImmunities(const HeatParameters::Parser& parser)
	{
		std::vector<const char*> copVehicles; // for game compatibility
		std::vector<bool>        isAffecteds;

		parser.ParseUser<const char*, bool>("Vehicles:Breakers", copVehicles, {isAffecteds});

		return copTypeToIsBreakerImmune.FillFromVectors
		(
			"Vehicle-to-immunity",
			Globals::GetVaultKey(HeatParameters::configDefaultKey),
			ModContainers::FillSetup(copVehicles, Globals::GetVaultKey, Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(isAffecteds, std::logical_not<bool>{})
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Fixes update for passive bounty amount after race pursuits
		MemoryTools::MakeRangeJMP<heatUpdateEntrance, heatUpdateExit>(HeatUpdate);

		// Fixes broken scene-selection logic for arrests
		MemoryTools::MakeRangeJMP<arrestSceneEntrance, arrestSceneExit>(ArrestScene);

		// Also fixes getting (hidden) BUSTED progress while the green EVADE bar fills
		MemoryTools::MakeRangeJMP<maxBustDistanceEntrance, maxBustDistanceExit>(MaxBustDistance);

		// Fixes incorrect array values read from VltEd
		MemoryTools::MakeRangeJMP<heatEscalationEntrance,    heatEscalationExit>   (HeatEscalation);
		MemoryTools::MakeRangeJMP<destructionBountyEntrance, destructionBountyExit>(DestructionBounty);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [GEN] GeneralSettings");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "General.ini")) return false;

		// Race tracking (and code modifications)
		ParseTrackingSettings(parser);

		// Heat parameters
		HeatParameters::Parse(parser, "Pursuits:Rivals", rivalPursuitsEnableds);

		HeatParameters::Parse(parser, "Bounty:Interval", bountyIntervals);
		HeatParameters::Parse(parser, "Bounty:Combo",    maxBountyMultipliers);

		HeatParameters::Parse(parser, "State:Busting",  bustTimers,            maxBustDistances);
		HeatParameters::Parse(parser, "State:Evading",  evadeTimers);
		HeatParameters::Parse(parser, "Evading:Hiding", carsAffectedByHidings, helisAffectedByHidings);

		HeatParameters::Parse(parser, "Flipping:Damaged", copFlipByDamageEnableds);
		HeatParameters::Parse(parser, "Flipping:Time",    copFlipByTimers);
		HeatParameters::Parse(parser, "Flipping:Reset",   racerFlipResetDelays);

		// Pursuit-breaker immunity
		if (ParsePursuitBreakerImmunities(parser))
		{
			// Code modifications (feature-specific)
			MemoryTools::MakeRangeJMP<pursuitBreakerCheckEntrance, pursuitBreakerCheckExit>(PursuitBreakerCheck);
		}

		// Code modifications (general)
		MemoryTools::Write<float*>(&bountyFrequency, {0x444513, 0x444524});

		MemoryTools::Write<float*>(&bustRate,             {0x40AEDB});
		MemoryTools::Write<float*>(&resetBustScale,       {0x4444D2});
		MemoryTools::Write<float*>(&recoveryBustDelta,    {0x4444E6});
		MemoryTools::Write<float*>(&(bustTimers.current), {0x4445CE});
		
		MemoryTools::Write<float*>(&halfEvadeRate,         {0x444A3A});
		MemoryTools::Write<float*>(&(evadeTimers.current), {0x4448E6, 0x444802, 0x4338F8});

		MemoryTools::MakeRangeJMP<copComboEntrance,              copComboExit>             (CopCombo);
		MemoryTools::MakeRangeJMP<copFlippingEntrance,           copFlippingExit>          (CopFlipping);
		MemoryTools::MakeRangeJMP<rivalPursuitEntrance,          rivalPursuitExit>         (RivalPursuit);
		MemoryTools::MakeRangeJMP<racerFlippingEntrance,         racerFlippingExit>        (RacerFlipping);
		MemoryTools::MakeRangeJMP<passiveBountyEntrance,         passiveBountyExit>        (PassiveBounty);
		MemoryTools::MakeRangeJMP<hiddenFromCarsEntrance,        hiddenFromCarsExit>       (HiddenFromCars);
		MemoryTools::MakeRangeJMP<hiddenFromRoadblocksEntrance,  hiddenFromRoadblocksExit> (HiddenFromRoadblocks);
		MemoryTools::MakeRangeJMP<hiddenFromHelicoptersEntrance, hiddenFromHelicoptersExit>(HiddenFromHelicopters);

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

		bustRate          = 1.f / bustTimers.current;
		resetBustScale    = std::max<float>(bustTimers.current / 1.25f, 4.f);
		recoveryBustDelta = -.25f * std::max<float>(bustTimers.current / 2.5f, 2.f);

		evadeTimers.SetToHeat(isRacing, heatLevel);

		halfEvadeRate = .5f / evadeTimers.current;

		carsAffectedByHidings .SetToHeat(isRacing, heatLevel);
		helisAffectedByHidings.SetToHeat(isRacing, heatLevel);

		copFlipByDamageEnableds.SetToHeat(isRacing, heatLevel);
		copFlipByTimers        .SetToHeat(isRacing, heatLevel);
		racerFlipResetDelays   .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}