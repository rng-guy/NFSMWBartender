#pragma once

#include <vector>
#include <concepts>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace GroundSuppport
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[SUP]";
	constexpr Globals::LogLiteral logName = "GroundSuppport";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, rivalRoadblockEnabled, true);
	constinit HEAT_PARAMETER_VALUE(bool, rivalHeavyEnabled,     true);
	constinit HEAT_PARAMETER_VALUE(bool, rivalLeaderEnabled,    true);

	constinit HEAT_PARAMETER_INTERVAL(float, roadblockCooldown,      8.f,  12.f, {1.f}); // seconds
	constinit HEAT_PARAMETER_VALUE   (float, roadblockHeavyCooldown, 15.f,       {1.f}); // seconds

	constinit HEAT_PARAMETER_INTERVAL(float, roadblockSpawnDistance, 250.f, 250.f, {0.f, 400.f}); // metres

	constinit HEAT_PARAMETER_VALUE(bool, roadblockEndsFormation, true);

	constinit HEAT_PARAMETER_INTERVAL(float, strategyCooldown, 10.f, 10.f, {1.f}); // seconds

	constinit HEAT_PARAMETER_VALUE(float, heavy3SpeedLimit, 100.f, {0.f}); // kph

	constinit HEAT_PARAMETER_VALUE(bool, heavy3TriggerCooldown, true);
	constinit HEAT_PARAMETER_VALUE(bool, heavy3AreBlockable,    true);

	constinit HEAT_PARAMETER_VALUE(const char*, heavy3LightVehicle, "copsuvl");
	constinit HEAT_PARAMETER_VALUE(const char*, heavy3HeavyVehicle, "copsuv");

	constinit HEAT_PARAMETER_VALUE(const char*, heavy4LightVehicle, "copsuvl");
	constinit HEAT_PARAMETER_VALUE(const char*, heavy4HeavyVehicle, "copsuv");

	constinit HEAT_PARAMETER_VALUE(const char*, leader5CrossVehicle ,"copcross");

	constinit HEAT_PARAMETER_VALUE(const char*, leader7CrossVehicle,  "copcross");
	constinit HEAT_PARAMETER_VALUE(const char*, leader7Hench1Vehicle, "copsporthench");
	constinit HEAT_PARAMETER_VALUE(const char*, leader7Hench2Vehicle, "copsporthench");

	// Parameter conversions
	float rammingSpeedLimit; // metres / second





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const char* __fastcall SelectHeavyVehicle(const address heavyStrategy)
	{
		const int strategyID = AsReference<int>(heavyStrategy);

		const int  heavyChance = AsReference<int>(heavyStrategy + 0xC);
		const bool isHeavy     = Globals::pRNG.DoPercentTrial<int>(heavyChance);

		switch (strategyID)
		{
		case 3: // ramming SUVs
			return ((isHeavy) ? heavy3HeavyVehicle : heavy3LightVehicle).current;

		case 4: // SUV roadblock
			return ((isHeavy) ? heavy4HeavyVehicle : heavy4LightVehicle).current;
		}

		return (isHeavy) ? "copsuv" : "copsuvl";
	}



	[[nodiscard]] const char* __fastcall SelectCrossVehicle(const address leaderStrategy)
	{
		const int strategyID = AsReference<int>(leaderStrategy);

		switch (strategyID)
		{
		case 5: // Cross only
			return leader5CrossVehicle.current;

		case 7: // Cross with henchmen
			return leader7CrossVehicle.current;
		}

		return "copcross";
	}



	[[nodiscard]] bool IsHeavyStrategyAvailable
	(
		const address pursuit,
		const address heavyStrategy
	) {
		const int  strategyID   = AsReference<int>    (heavyStrategy);
		const bool hasRoadblock = AsReference<address>(pursuit + 0x84);

		switch (strategyID)
		{
		case 3: // ramming SUVs
			return (not (hasRoadblock and heavy3AreBlockable.current));

		case 4: // SUV roadblock
			return (not hasRoadblock);
		}

		return false;
	}



	[[nodiscard]] bool IsLeaderStrategyAvailable
	(
		const address pursuit,
		const address leaderStrategy
	) {
		const int crossFlag = AsReference<int>(pursuit + 0x164);
		if (crossFlag != 0) return false; // active or blocked

		const int strategyID = AsReference<int>(leaderStrategy);

		switch (strategyID)
		{
		case 5: // Cross only
		case 7: // Cross with henchmen
			return true;
		}

		return false;
	}

	

	void __fastcall ReportPriorityOutcome(const address pursuit)
	{
		if constexpr (Globals::loggingEnabled)
		{
			const address leaderStrategy = AsReference<address>(pursuit + 0x198);
			const int     strategyID     = AsReference<int>    (leaderStrategy);

			Globals::LogFull(pursuit, logTag, "Priority: LeaderStrategy", strategyID);
		}
	}



	template <address GetCount, address GetEntry, auto IsAvailable>
	requires std::predicate<decltype(IsAvailable), address, address>
	void MarshalStrategies
	(
		const address         pursuit,
		std::vector<address>& candidates
	) {
		const auto GetSupportNode = AsFunction<address __thiscall (address)>(0x418EE0);

		const address supportNode = GetSupportNode(pursuit - 0x48);
		ASSERT_CONDITION_THEN_IF_FALSE(supportNode, return);

		const auto GetNumStrategies = AsFunction<size_t  __thiscall (address)>        (GetCount);
		const auto GetStrategy      = AsFunction<address __thiscall (address, size_t)>(GetEntry);

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; ++strategyID)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsAvailable(pursuit, strategy)) continue;

			const int chance = AsReference<int>(strategy + 0x4);

			if (Globals::pRNG.DoPercentTrial<int>(chance))
				candidates.push_back(strategy);
		}
	}



	void SetStrategy
	(
		const address pursuit,
		const address strategy,
		const bool    isHeavyStrategy
	) {
		const float duration = AsReference<float>(strategy + 0x8);

		AsReference<float>(pursuit + 0x208) = duration; // strategy duration
		AsReference<int>  (pursuit + 0x20C) = 1;        // request flag

		if (not isHeavyStrategy)
		{
			AsReference<address>(pursuit + 0x198) = strategy;

			return; // is LeaderStrategy
		}

		const int strategyID = AsReference<int>(strategy);

		if ((strategyID != 3) or heavy3TriggerCooldown.current)
			AsReference<float>(pursuit + 0xC8) = roadblockHeavyCooldown.current; // roadblock cooldown

		AsReference<address>(pursuit + 0x194) = strategy; // HeavyStrategy
	}



	[[nodiscard]] bool __fastcall SetRandomStrategy(const address pursuit) 
	{
		static RELEASE_CONSTINIT std::vector<address> candidates;

		// Marshal all currently eligible Strategies
		const bool isPlayerPursuit = Globals::IsPlayerPursuit(pursuit);

		if (isPlayerPursuit or rivalHeavyEnabled.current)
			MarshalStrategies<0x403600, 0x4035E0, IsHeavyStrategyAvailable>(pursuit, candidates);

		const size_t numHeavyStrategies = candidates.size();

		if (isPlayerPursuit or rivalLeaderEnabled.current)
			MarshalStrategies<0x403680, 0x403660, IsLeaderStrategyAvailable>(pursuit, candidates);

		// Check candidate count
		if (candidates.empty())
		{
			if constexpr (Globals::loggingEnabled)
			{
				const bool canMakeRequest = (isPlayerPursuit or rivalHeavyEnabled.current or rivalLeaderEnabled.current);
				Globals::LogFull(pursuit, logTag, "Strategy request failed", (canMakeRequest) ? "(chance)" : "(blocked)");
			}

			return false; // no candidates
		}

		// Select an eligible Strategy at random
		const size_t  candidateID     = Globals::pRNG.GenerateIndex(candidates);
		const address randomStrategy  = candidates[candidateID];
		const bool    isHeavyStrategy = (candidateID < numHeavyStrategies);

		SetStrategy(pursuit, randomStrategy, isHeavyStrategy);

		if constexpr (Globals::loggingEnabled)
		{
			const int strategyID = AsReference<int>(randomStrategy);
			Globals::LogFull(pursuit, logTag, "Requesting", (isHeavyStrategy) ? "HeavyStrategy" : "LeaderStrategy", strategyID);

			Globals::LogPlain("Candidate", Globals::LogDec(candidateID + 1), '/', Globals::LogDec(candidates.size()));
		}

		candidates.clear();

		return true;
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Updates the Cross flag whenever he joins a given pursuit
	ASSEMBLY_DETOUR(OnAttached, /* begin = */ 0x424036, /* end = */ 0x42403C)
	{
		__asm
		{
			mov edx, dword ptr [edi + 0x54]     // AIVehicle
			cmp byte ptr [edx - 0x4C + 0x83], 1 // padding byte: Cross flag (car)
			jne conclusion                      // not Cross' vehicle

			mov dword ptr [esi + 0x174], 1 // Cross flag (pursuit)
			
			conclusion:
			// Execute original code and resume
			fild dword ptr [esi + 0x148]

			EXIT_ASSEMBLY_DETOUR(OnAttached)
		}
	}



	// Checks whether a despawning vehicle was Cross
	ASSEMBLY_DETOUR(OnDetached, 0x42B5F1, 0x42B616)
	{
		__asm
		{
			// Execute original code first
			mov ecx, esi
			mov edx, dword ptr [esi]

			mov eax, dword ptr [esi + 0x54]     // AIVehicle
			cmp byte ptr [eax - 0x4C + 0x83], 1 // padding byte: Cross flag (car)

			EXIT_ASSEMBLY_DETOUR(OnDetached)
		}
	}



	// Marks Cross' replacement for later identification
	ASSEMBLY_DETOUR(CrossSpawn, 0x41F7D9, 0x41F7E0)
	{
		__asm
		{
			mov eax, dword ptr [esp + 0x94] // final Strategy vehicle
			mov edx, dword ptr [eax + 0xC]  // current Strategy vehicle

			cmp ebp, edx
			sete byte ptr [ebx - 0x4C + 0x83] // padding byte: Cross flag (car)

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x90]

			EXIT_ASSEMBLY_DETOUR(CrossSpawn)
		}
	}



	// Replaces henchmen vehicles before they spawn
	ASSEMBLY_DETOUR(HenchmenSub, 0x41F485, 0x41F497)
	{
		__asm
		{
			push esi

			mov eax, dword ptr [leader7Hench1Vehicle.current]
			mov edx, dword ptr [leader7Hench2Vehicle.current]

			// Only LeaderStrategy 7 reads these
			mov dword ptr [esp + 0x24], eax
			mov dword ptr [esp + 0x28], edx

			mov esi, dword ptr [esp + 0x60]

			EXIT_ASSEMBLY_DETOUR(HenchmenSub)
		}
	}



	// Replaces HeavyStrategy vehicles before they spawn
	ASSEMBLY_DETOUR(HeavySelector, 0x41F1A4, 0x41F1C8)
	{
		__asm
		{
			mov ecx, dword ptr [esi]
			call SelectHeavyVehicle // ecx: heavyStrategy

			EXIT_ASSEMBLY_DETOUR(HeavySelector)
		}
	}



	// Replaces Cross' vehicle before he spawns
	ASSEMBLY_DETOUR(CrossSelector, 0x41F504, 0x41F50C)
	{
		__asm
		{
			mov edi, eax

			mov ecx, dword ptr [esi + 0x4]
			call SelectCrossVehicle // ecx: LeaderStrategy
			mov dword ptr [esp + 0x24], eax

			test edi, edi
			mov eax, edi

			EXIT_ASSEMBLY_DETOUR(CrossSelector)
		}
	}



	// Can skip the Cross priority request in rival pursuits
	ASSEMBLY_DETOUR(CrossPriority, 0x419724, 0x41972A)
	{
		static constexpr address disabledExit = 0x419780;

		__asm
		{
			jne disabled // priority flag set

			cmp byte ptr [rivalLeaderEnabled.current], 1
			je conclusion // no rival discrimination

			mov ecx, esi
			call Globals::IsPlayerPursuit
			test al, al
			je disabled // not player pursuit

			conclusion:
			// Execute original code and resume
			mov ecx, ebp
			xor ebx, ebx

			EXIT_ASSEMBLY_DETOUR(CrossPriority)

			disabled:
			jmp dword ptr [disabledExit]
		}
	}



	// Can skip roadblock requests in rival pursuits
	ASSEMBLY_DETOUR(RivalRoadblock, 0x419563, 0x419568)
	{
		static constexpr address disabledExit = 0x4195CD;

		__asm
		{
			cmp byte ptr [rivalRoadblockEnabled.current], 1
			je conclusion // no rival discrimination

			call Globals::IsPlayerPursuit
			test al, al
			je disabled // not player pursuit

			mov ecx, esi
			mov edx, dword ptr [esi]

			conclusion:
			// Execute original code and resume
			call dword ptr [edx + 0x28] // AIPursuit::IsPerpInSight
			cmp al, 1

			EXIT_ASSEMBLY_DETOUR(RivalRoadblock)

			disabled:
			jmp dword ptr [disabledExit]
		}
	}



	// Logs the outcome of LeaderStrategy priority checks
	ASSEMBLY_DETOUR(PriorityOutcome, 0x419770, 0x419776)
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0x208], eax

			mov ecx, esi
			call ReportPriorityOutcome // ecx: pursuit

			EXIT_ASSEMBLY_DETOUR(PriorityOutcome)
		}
	}



	// Updates the Strategy cooldown after each request
	ASSEMBLY_DETOUR(RequestCooldown, 0x4196D7, 0x4196E4)
	{
		__asm
		{
			mov ecx, offset strategyCooldown
			call HeatParameters::Interval<float>::GetRandomValue
			fstp dword ptr [esi + 0x210] // strategy cooldown

			// Execute original code and resume
			lea ecx, dword ptr [esi - 0x48]

			EXIT_ASSEMBLY_DETOUR(RequestCooldown)
		}
	}



	// Sets the initial speed of HeavyStrategy 3 vehicles
	ASSEMBLY_DETOUR(HeavySpeedSetup, 0x421526, 0x421545)
	{
		__asm
		{
			fld dword ptr [rammingSpeedLimit]
			fcom st(1)
			fnstsw ax
			test ah, 0x5
			je conclusion // not above limit

			fxch st(1)

			conclusion:
			fstp st(0)
			fstp dword ptr [esp + 0x4]

			EXIT_ASSEMBLY_DETOUR(HeavySpeedSetup)
		}
	}



	// Updates the speed of HeavyStrategy 3 vehicles
	ASSEMBLY_DETOUR(HeavySpeedUpdate, 0x4215F6, 0x4215FB)
	{
		__asm
		{
			push dword ptr [rammingSpeedLimit]

			EXIT_ASSEMBLY_DETOUR(HeavySpeedUpdate)
		}
	}



	// Updates the non-Strategy roadblock cooldown after each request
	ASSEMBLY_DETOUR(RoadblockCooldown, 0x419535, 0x41954C)
	{
		__asm
		{
			mov ecx, offset roadblockCooldown
			call HeatParameters::Interval<float>::GetRandomValue

			EXIT_ASSEMBLY_DETOUR(RoadblockCooldown)
		}
	}



	// Changes the distance at which roadblocks can spawn from racers
	ASSEMBLY_DETOUR(RoadblockDistance, 0x43DE45, 0x43DE4A)
	{
		__asm
		{
			push ecx

			mov ecx, offset roadblockSpawnDistance
			call HeatParameters::Interval<float>::GetRandomValue

			mov ecx, dword ptr [esp]
			fstp dword ptr [esp]

			EXIT_ASSEMBLY_DETOUR(RoadblockDistance)
		}
	}



	// Selects a random available Strategy without any biases
	ASSEMBLY_DETOUR(StrategySelection, 0x41978D, 0x41984E)
	{
		__asm
		{
			mov ecx, esi
			call SetRandomStrategy // ecx: pursuit
			movzx eax, al

			xor edi, edi
			sub edi, eax

			EXIT_ASSEMBLY_DETOUR(StrategySelection)
		}
	}



	// Can prevent roadblock spawns from cancelling cop formations
	ASSEMBLY_DETOUR(RoadblockFormation, 0x40AE5A, 0x40AE63)
	{
		__asm
		{
			cmp byte ptr [roadblockEndsFormation.current], 1
			jne conclusion // keep formation

			// Execute original code and resume
			mov eax, dword ptr [esi]
			mov ecx, esi
			call dword ptr [eax + 0x58] // AIPursuit::IsFinisherActive
			test al, al

			conclusion:
			EXIT_ASSEMBLY_DETOUR(RoadblockFormation)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	void UpdateParameterConversions()
	{
		rammingSpeedLimit = heavy3SpeedLimit.current / 3.6f;
	}



	void ResolveAllVehicleNames()
	{
		bool allTypesValid = true;

		constexpr std::array vehicleNames = 
		{
			&heavy3LightVehicle,  
			&heavy3HeavyVehicle, 
			&heavy4LightVehicle,  
			&heavy4HeavyVehicle,
			&leader5CrossVehicle,
			&leader7CrossVehicle, 
			&leader7Hench1Vehicle, 
			&leader7Hench2Vehicle
		};

		for (auto* const vehicleName : vehicleNames)
			allTypesValid &= HeatParameters::ResolveCarNames(*vehicleName);

		if constexpr (Globals::loggingEnabled)
		{
			if (allTypesValid)
				Globals::LogPlain("All vehicles valid");
		}
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Biases in the Strategy-selection process
		PATCH_ASSEMBLY_DETOUR(StrategySelection);
	}



	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileSupport)) return false;

		// Heat parameters
		HeatParameters::Extract(parser, "Support:Rivals", rivalRoadblockEnabled, rivalHeavyEnabled, rivalLeaderEnabled);

		HeatParameters::Extract(parser, "Roadblocks:Cooldown", roadblockCooldown, roadblockHeavyCooldown);

		HeatParameters::Extract(parser, "Roadblocks:Distance", roadblockSpawnDistance);

		HeatParameters::Extract(parser, "Roadblocks:Formations", roadblockEndsFormation);

		HeatParameters::Extract(parser, "Strategies:Cooldown", strategyCooldown);

		HeatParameters::Extract(parser, "Heavy3:Speed", heavy3SpeedLimit);

		HeatParameters::Extract(parser, "Heavy3:Roadblocks", heavy3TriggerCooldown, heavy3AreBlockable);

		HeatParameters::Extract(parser, "Heavy3:Vehicles", heavy3LightVehicle, heavy3HeavyVehicle);

		HeatParameters::Extract(parser, "Heavy4:Vehicles", heavy4LightVehicle, heavy4HeavyVehicle);

		HeatParameters::Extract(parser, "Leader5:Vehicle", leader5CrossVehicle);

		HeatParameters::Extract(parser, "Leader7:Vehicles", leader7CrossVehicle, leader7Hench1Vehicle, leader7Hench2Vehicle);

		// Parameter conversions
		UpdateParameterConversions(); // uses vanilla value(s)

		// Check and make vehicle names persistent
		ResolveAllVehicleNames();

		// Code modifications (geneal)
		MemoryTools::MakeRangeNOP<0x42402A, 0x424036>(); // Cross flag = 1

		PATCH_ASSEMBLY_DETOUR(OnAttached);
		PATCH_ASSEMBLY_DETOUR(OnDetached);
		PATCH_ASSEMBLY_DETOUR(CrossSpawn);
		PATCH_ASSEMBLY_DETOUR(HenchmenSub);
		PATCH_ASSEMBLY_DETOUR(HeavySelector);
		PATCH_ASSEMBLY_DETOUR(CrossSelector);
		PATCH_ASSEMBLY_DETOUR(CrossPriority);
		PATCH_ASSEMBLY_DETOUR(RivalRoadblock);
		PATCH_ASSEMBLY_DETOUR(RequestCooldown);
		PATCH_ASSEMBLY_DETOUR(HeavySpeedSetup);
		PATCH_ASSEMBLY_DETOUR(HeavySpeedUpdate);
		PATCH_ASSEMBLY_DETOUR(RoadblockCooldown);
		PATCH_ASSEMBLY_DETOUR(RoadblockDistance);
		PATCH_ASSEMBLY_DETOUR(RoadblockFormation);

		// Code modifications (logging)
		if constexpr (Globals::loggingEnabled)
			PATCH_ASSEMBLY_DETOUR(PriorityOutcome);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}

	

	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		// Heat parameters
		rivalRoadblockEnabled.SetToHeatState(state);
		rivalHeavyEnabled    .SetToHeatState(state);
		rivalLeaderEnabled   .SetToHeatState(state);

		roadblockCooldown     .SetToHeatState(state);
		roadblockHeavyCooldown.SetToHeatState(state);

		roadblockSpawnDistance.SetToHeatState(state);
		roadblockEndsFormation.SetToHeatState(state);

		strategyCooldown.SetToHeatState(state);

		heavy3SpeedLimit.SetToHeatState(state);

		heavy3TriggerCooldown.SetToHeatState(state);
		heavy3AreBlockable   .SetToHeatState(state);

		heavy3LightVehicle.SetToHeatState(state);
		heavy3HeavyVehicle.SetToHeatState(state);

		heavy4LightVehicle.SetToHeatState(state);
		heavy4HeavyVehicle.SetToHeatState(state);

		leader5CrossVehicle.SetToHeatState(state);

		leader7CrossVehicle .SetToHeatState(state);
		leader7Hench1Vehicle.SetToHeatState(state);
		leader7Hench2Vehicle.SetToHeatState(state);

		// Parameter conversions
		UpdateParameterConversions();
	}
}