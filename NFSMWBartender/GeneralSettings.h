#pragma once

#include <span>
#include <array>
#include <vector>
#include <algorithm>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace GeneralSettings 
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[GEN]";
	constexpr LogLiteral logName = "GeneralSettings";

	// Pursuit behaviour
	bool trackPursuitLength  = false;
	bool trackUnitsInPursuit = false;
	bool trackCopsLost       = false;
	bool trackCopsDamaged    = false;
	bool trackCopsDestroyed  = false;
	bool trackPassiveBounty  = false;
	bool trackPropertyDamage = false;
	bool trackInfractions    = false;

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, rivalPursuitsEnabled, true);

	constinit HEAT_PARAMETER_VALUE(float, bountyInterval,      10.f, {.001f}); // seconds
	constinit HEAT_PARAMETER_VALUE(int,   maxBountyMultiplier, 3,    {1});     // scale

	constinit HEAT_PARAMETER_VALUE(float, bustTimer,       5.f,  {.001f}); // seconds
	constinit HEAT_PARAMETER_VALUE(float, maxBustDistance, 15.f, {0.f});   // metres

	constinit HEAT_PARAMETER_VALUE(float, evadeTimer, 7.f, {.001f}); // seconds

	constinit HEAT_PARAMETER_VALUE(bool, carsAffectedByHiding,  true);
	constinit HEAT_PARAMETER_VALUE(bool, helisAffectedByHiding, true);

	constinit HEAT_PARAMETER_VALUE(bool, copFlipByDamageEnabled, true);

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(float, copFlipByTimer,     {0.f}); // seconds
	constinit OPTIONAL_HEAT_PARAMETER_VALUE(float, racerFlipResetDelay,{0.f}); // seconds

	// Conversions
	float bountyFrequency = 1.f / bountyInterval.current; // hertz
	
	float bustRate          = 1.f / bustTimer.current; // hertz
	float resetBustScale    = std::max<float>(bustTimer.current / 1.25f, 4.f);
	float recoveryBustDelta = -.25f * std::max<float>(bustTimer.current / 2.5f, 2.f);

	float halfEvadeRate = .5f / evadeTimer.current; // hertz

	// Code caves
	RELEASE_CONSTINIT DEFAULT_VAULT_MAP(bool, copTypeToIsBreakerImmune, false);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const char* __fastcall GetRandomArrestScene(const size_t heatLevel)
	{
		static constexpr std::array<const char*, 8> scenesDefault =
		{
			"ArrestM06",  "ArrestM19",  "ArrestF06",  "ArrestF07",
			"ArrestM06b", "ArrestM19b", "ArrestF06b", "ArrestF07b"
		};

		static constexpr std::array<const char*, 8> scenesLevel1 =
		{
			"ArrestM01",  "ArrestM16",  "ArrestF02",  "ArrestF18",
			"ArrestM01b", "ArrestM16b", "ArrestF02b", "ArrestF18b"
		};

		static constexpr std::array<const char*, 4> scenesLevel2 =
		{
			"ArrestM04",  "ArrestF23",
			"ArrestM04b", "ArrestF23b"
		};

		static constexpr std::array<const char*, 6> scenesLevel3 =
		{
			"ArrestM07",  "ArrestM14",  "ArrestF14",  
			"ArrestM07b", "ArrestM14b", "ArrestF14b"
		};

		// Select random cutscene by Heat level
		std::span<const char* const> candidates = scenesDefault;

		switch (heatLevel)
		{
		case 0:
		case 1:
			candidates = scenesLevel1;
			break;

		case 2:
			candidates = scenesLevel2;
			break;

		case 3:
			candidates = scenesLevel3;
		}

		const size_t      sceneID     = Globals::prng.GenerateIndex(candidates);
		const char* const randomScene = candidates[sceneID];

		if constexpr (Globals::loggingEnabled)
			Globals::LogTagged(logTag, "Arrest scene:", randomScene);

		return randomScene;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copComboEntrance = 0x418FBA;
	constexpr address copComboExit     = 0x418FD2;

	// Updates the combo-bounty counter
	__declspec(naked) void CopCombo()
	{
		__asm
		{
			mov ecx, dword ptr [maxBountyMultiplier.current]
			mov eax, dword ptr [esi + 0xF0] // combo count

			inc eax
			cmp eax, ecx
			cmovl ecx, eax // count below maximum

			mov dword ptr [esi + 0xF0], ecx

			jmp dword ptr [copComboExit]
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
			jmp dword ptr [heatUpdateExit]
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
			cmp byte ptr [copFlipByTimer.isEnabled.current], 1
			jne damaged // time check disabled

			fld dword ptr [esi + 0xB8] // time spent flipped
			fcomp dword ptr [copFlipByTimer.value.current]
			fnstsw ax
			test ah, 0x41
			jne damaged                // delay has yet to expire

			mov eax, dword ptr [esi + 0x4C]
			lea ecx, dword ptr [esi + 0x4C]
			call dword ptr [eax + 0x1C] // DamageVehicle::Destroy
			jmp conclusion              // cop was destroyed

			damaged:
			cmp byte ptr [copFlipByDamageEnabled.current], 1
			jne skip // damage check disabled

			conclusion:
			jmp dword ptr [copFlippingExit]

			skip:
			jmp dword ptr [copFlippingSkip]
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

			jmp dword ptr [arrestSceneExit]
		}
	}



	constexpr address rivalPursuitEntrance = 0x426C9C;
	constexpr address rivalPursuitExit     = 0x426CA4;

	// Decides whether cops can start non-player pursuits
	__declspec(naked) void RivalPursuit()
	{
		__asm
		{
			cmp byte ptr [rivalPursuitsEnabled.current], 0
			je conclusion // rival pursuits disabled

			// Execute original and resume
			mov ecx, dword ptr [eax + 0x1968]
			test ecx, ecx

			conclusion:
			jmp dword ptr [rivalPursuitExit]
		}
	}



	constexpr address racerFlippingEntrance = 0x6A45A3;
	constexpr address racerFlippingExit     = 0x6A45B1;

	// Checks whether flipped racers should be reset
	__declspec(naked) void RacerFlipping()
	{
		__asm
		{
			cmp byte ptr [racerFlipResetDelay.isEnabled.current], 1
			jne conclusion // flipping resets disabled

			fld dword ptr [esi + 0x54] // time spent flipped
			fcomp dword ptr [racerFlipResetDelay.value.current]
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr [racerFlippingExit]
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
			jmp dword ptr [passiveBountyExit]
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
			fld dword ptr [maxBustDistance.current]
			
			// Execute original code and resume
			conclusion:
			test bl, bl

			jmp dword ptr [maxBustDistanceExit]
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

			jmp dword ptr [heatEscalationExit]
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

			jmp dword ptr [destructionBountyExit]
		}
	}



	constexpr address hiddenFromCarsEntrance = 0x416571;
	constexpr address hiddenFromCarsExit     = 0x41657A;

	// Decides whether racers are invisible to cop cars
	__declspec(naked) void HiddenFromCars()
	{
		__asm
		{
			mov al, byte ptr [carsAffectedByHiding.current]
			test al, byte ptr [edi + 0x2C] // hidden from cars

			jmp dword ptr [hiddenFromCarsExit]
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
			call ModContainers::DefaultVaultMap<bool>::GetValue
			test al, al

			conclusion:
			jmp dword ptr [pursuitBreakerCheckExit]
		}
	}



	constexpr address hiddenFromRoadblocksEntrance = 0x444329;
	constexpr address hiddenFromRoadblocksExit     = 0x444333;

	// Decides whether racers are invisible to roadblocks
	__declspec(naked) void HiddenFromRoadblocks()
	{
		__asm
		{
			mov al, byte ptr [carsAffectedByHiding.current]
			test al, byte ptr [ebp + 0x2C] // hidden from cars

			jmp dword ptr [hiddenFromRoadblocksExit]
		}
	}



	constexpr address hiddenFromHelicoptersEntrance = 0x417103;
	constexpr address hiddenFromHelicoptersExit     = 0x41710C;

	// Decides whether racers are invisible to helicopters
	__declspec(naked) void HiddenFromHelicopters()
	{
		__asm
		{
			mov al, byte ptr [helisAffectedByHiding.current]
			test al, byte ptr [esi + 0x2D] // hidden from helicopters

			jmp dword ptr [hiddenFromHelicoptersExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ParseTrackingSettings(const HeatParameters::Parser& parser)
	{
		const auto ParseTracking = [&parser](const std::string_view key, bool& isTracked) -> bool
		{
			parser.ParseFromFile<bool>("Pursuits:Races", key, {isTracked});
			return isTracked; // for immediate toggle checking
		};

		// Pursuit length
		if (ParseTracking("pursuitLength", trackPursuitLength))
		{
			MemoryTools::MakeRangeNOP<0x443CBE, 0x443CC8>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking pursuit length");
		}

		// Units in pursuit
		if (ParseTracking("unitsInPursuit", trackUnitsInPursuit))
		{
			MemoryTools::MakeRangeNOP<0x41911B, 0x419125>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking units in pursuit");
		}

		// Cops lost
		if (ParseTracking("copsLost", trackCopsLost))
		{
			MemoryTools::MakeRangeNOP<0x42B761, 0x42B76B>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking cops lost");
		}

		// Cops damaged
		if (ParseTracking("copsDamaged", trackCopsDamaged))
		{
			MemoryTools::MakeRangeNOP<0x40AF43, 0x40AF4D>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking cops damaged");
		}

		// Cops destroyed
		if (ParseTracking("copsDestroyed", trackCopsDestroyed))
		{
			MemoryTools::MakeRangeNOP<0x4094E0, 0x4094EA>(); // cop bounty
			MemoryTools::MakeRangeNOP<0x418F3B, 0x418F41>(); // cops destroyed
			MemoryTools::MakeRangeNOP<0x43EA15, 0x43EA19>(); // total cops destroyed

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking cops destroyed");
		}

		// Passive bounty
		if (ParseTracking("passiveBounty", trackPassiveBounty))
		{
			MemoryTools::MakeRangeNOP<0x4094A0, 0x4094AA>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking passive bounty");
		}

		// Property damage
		if (ParseTracking("propertyDamage", trackPropertyDamage))
		{
			MemoryTools::MakeRangeNOP<0x409463, 0x409467>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking property damage");
		}

		// Infractions
		if (ParseTracking("infractions", trackInfractions))
		{
			MemoryTools::MakeRangeNOP<0x5FDDDC, 0x5FDDE7>();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Tracking infractions");
		}
	}



	bool ParsePursuitBreakerImmunities(const HeatParameters::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<bool>             isBreakerImmunes;

		parser.ParseUser<std::string_view, bool>("Vehicles:Breakers", copNames, {isBreakerImmunes});

		return copTypeToIsBreakerImmune.Fill
		(
			HeatParameters::configDefaultKey,
			ModContainers::FillSetup(copNames,         Globals::GetVaultHash,         Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(isBreakerImmunes, ModContainers::IdentityCopy(), ModContainers::AlwaysValid())
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



	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.LoadFile(HeatParameters::configPathBasic, "General.ini")) return false;

		// Race tracking (and code modifications)
		ParseTrackingSettings(parser);

		// Heat parameters
		HeatParameters::Parse(parser, "Pursuits:Rivals", rivalPursuitsEnabled);

		HeatParameters::Parse(parser, "Bounty:Interval", bountyInterval);

		HeatParameters::Parse(parser, "Bounty:Combo", maxBountyMultiplier);

		HeatParameters::Parse(parser, "State:Busting", bustTimer, maxBustDistance);

		HeatParameters::Parse(parser, "State:Evading", evadeTimer);

		HeatParameters::Parse(parser, "Evading:Hiding", carsAffectedByHiding, helisAffectedByHiding);

		HeatParameters::Parse(parser, "Flipping:Damaged", copFlipByDamageEnabled);

		HeatParameters::Parse(parser, "Flipping:Time", copFlipByTimer);

		HeatParameters::Parse(parser, "Flipping:Reset", racerFlipResetDelay);

		// Pursuit-breaker immunity
		if (ParsePursuitBreakerImmunities(parser))
		{
			// Code modifications (conditional)
			MemoryTools::MakeRangeJMP<pursuitBreakerCheckEntrance, pursuitBreakerCheckExit>(PursuitBreakerCheck);
		}

		// Code modifications (general)
		MemoryTools::Write<float*>(&bountyFrequency, {0x444513, 0x444524});

		MemoryTools::Write<float*>(&bustRate,            {0x40AEDB});
		MemoryTools::Write<float*>(&resetBustScale,      {0x4444D2});
		MemoryTools::Write<float*>(&recoveryBustDelta,   {0x4444E6});
		MemoryTools::Write<float*>(&(bustTimer.current), {0x4445CE});
		
		MemoryTools::Write<float*>(&halfEvadeRate,        {0x444A3A});
		MemoryTools::Write<float*>(&(evadeTimer.current), {0x4448E6, 0x444802, 0x4338F8});

		MemoryTools::MakeRangeJMP<copComboEntrance,              copComboExit>             (CopCombo);
		MemoryTools::MakeRangeJMP<copFlippingEntrance,           copFlippingExit>          (CopFlipping);
		MemoryTools::MakeRangeJMP<rivalPursuitEntrance,          rivalPursuitExit>         (RivalPursuit);
		MemoryTools::MakeRangeJMP<racerFlippingEntrance,         racerFlippingExit>        (RacerFlipping);
		MemoryTools::MakeRangeJMP<passiveBountyEntrance,         passiveBountyExit>        (PassiveBounty);
		MemoryTools::MakeRangeJMP<hiddenFromCarsEntrance,        hiddenFromCarsExit>       (HiddenFromCars);
		MemoryTools::MakeRangeJMP<hiddenFromRoadblocksEntrance,  hiddenFromRoadblocksExit> (HiddenFromRoadblocks);
		MemoryTools::MakeRangeJMP<hiddenFromHelicoptersEntrance, hiddenFromHelicoptersExit>(HiddenFromHelicopters);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		rivalPursuitsEnabled.SetToHeatState(state);

		bountyInterval.SetToHeatState(state);

		maxBountyMultiplier.SetToHeatState(state);

		bountyFrequency = 1.f / bountyInterval.current;

		bustTimer      .SetToHeatState(state);
		maxBustDistance.SetToHeatState(state);

		bustRate          = 1.f / bustTimer.current;
		resetBustScale    = std::max<float>(bustTimer.current / 1.25f, 4.f);
		recoveryBustDelta = -.25f * std::max<float>(bustTimer.current / 2.5f, 2.f);

		evadeTimer.SetToHeatState(state);

		halfEvadeRate = .5f / evadeTimer.current;

		carsAffectedByHiding .SetToHeatState(state);
		helisAffectedByHiding.SetToHeatState(state);

		copFlipByDamageEnabled.SetToHeatState(state);

		copFlipByTimer.SetToHeatState(state);

		racerFlipResetDelay.SetToHeatState(state);
	}
}