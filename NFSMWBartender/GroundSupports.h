#pragma once

#include <string>
#include <vector>
#include <concepts>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "CopSpawnOverrides.h"
#include "LeaderOverrides.h"



namespace GroundSupports
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Pair<bool> rivalRoadblockEnableds(true);
	constinit HeatParameters::Pair<bool> rivalHeavyEnableds    (true);
	constinit HeatParameters::Pair<bool> rivalLeaderEnableds   (true);

	constinit HeatParameters::Interval<float> roadblockCooldowns     (8.f,  12.f, {1.f}); // seconds
	constinit HeatParameters::Pair    <float> roadblockHeavyCooldowns(15.f, {1.f});       // seconds

	constinit HeatParameters::Interval<float> roadblockSpawnDistances(250.f, 250.f, {0.f}); // metres

	constinit HeatParameters::Pair<bool> roadblockEndsFormations(true);

	constinit HeatParameters::OptionalPair<float> regularRBJoinTimers({0.f}); // seconds
	constinit HeatParameters::OptionalPair<float> backupRBJoinTimers ({0.f}); // seconds

	constinit HeatParameters::Pair<float> maxRBJoinDistances      (500.f, {0.f}); // metres
	constinit HeatParameters::Pair<float> maxRBJoinElevationDeltas(1.5f,  {0.f}); // metres
	constinit HeatParameters::Pair<int>   maxRBJoinCounts         (1,     {0});   // cars

	constinit HeatParameters::Pair<bool> reactToCooldownModes(true);
	constinit HeatParameters::Pair<bool> reactToSpikesHits   (true);
	
	constinit HeatParameters::Interval<float> strategyCooldowns(10.f, 10.f, {1.f}); // seconds

	constinit HeatParameters::Pair<bool> heavy3TriggerCooldowns(true);
	constinit HeatParameters::Pair<bool> heavy3AreBlockables   (true);

	constinit HeatParameters::PointerPair<std::string> heavy3LightVehicles("copsuvl");
	constinit HeatParameters::PointerPair<std::string> heavy3HeavyVehicles("copsuv");

	constinit HeatParameters::PointerPair<std::string> heavy4LightVehicles("copsuvl");
	constinit HeatParameters::PointerPair<std::string> heavy4HeavyVehicles("copsuv");

	constinit HeatParameters::PointerPair<std::string> leader5CrossVehicles("copcross");
	constinit HeatParameters::PointerPair<std::string> leader7CrossVehicles("copcross");

	constinit HeatParameters::PointerPair<std::string> leader7Hench1Vehicles("copsporthench");
	constinit HeatParameters::PointerPair<std::string> leader7Hench2Vehicles("copsporthench");





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	const char* __fastcall SelectHeavyVehicle(const address heavyStrategy) 
	{
		const int strategyID = *reinterpret_cast<volatile int*>(heavyStrategy);

		const int  heavyChance = *reinterpret_cast<volatile int*>(heavyStrategy + 0xC);
		const bool isHeavy     = Globals::prng.DoPercentTrial<int>(heavyChance);

		switch (strategyID)
		{
		case 3: // ramming SUVs
			return (isHeavy) ? heavy3HeavyVehicles.current : heavy3LightVehicles.current;

		case 4: // SUV roadblock
			return (isHeavy) ? heavy4HeavyVehicles.current : heavy4LightVehicles.current;
		}

		return (isHeavy) ? "copsuv" : "copsuvl";
	}



	const char* __fastcall SelectCrossVehicle(const address leaderStrategy)
	{
		const int strategyID = *reinterpret_cast<volatile int*>(leaderStrategy);

		switch (strategyID)
		{
		case 5: // Cross only
			return leader5CrossVehicles.current;

		case 7: // Cross with henchmen
			return leader7CrossVehicles.current;
		}

		return "copcross";
	}



	bool IsHeavyStrategyAvailable
	(
		const address pursuit,
		const address heavyStrategy
	) {
		const int  strategyID   = *reinterpret_cast<volatile int*>    (heavyStrategy);
		const bool hasRoadblock = *reinterpret_cast<volatile address*>(pursuit + 0x84);

		switch (strategyID)
		{
		case 3: // ramming SUVs
			return (not (hasRoadblock and heavy3AreBlockables.current));

		case 4: // SUV roadblock
			return (not hasRoadblock);
		}

		return false;
	}



	bool IsLeaderStrategyAvailable
	(
		const address pursuit,
		const address leaderStrategy
	) {
		const int crossFlag = *reinterpret_cast<volatile int*>(pursuit + 0x164);

		if (crossFlag == 0)
		{
			const int strategyID = *reinterpret_cast<volatile int*>(leaderStrategy);

			switch (strategyID)
			{
			case 5: // Cross only
			case 7: // Cross with henchmen
				return true;
			}
		}

		return false;
	}

	

	void __fastcall ReportPriorityOutcome(const address pursuit)
	{
		if constexpr (Globals::loggingEnabled)
		{
			const address leaderStrategy = *reinterpret_cast<volatile address*>(pursuit + 0x198);
			const int     strategyID     = *reinterpret_cast<volatile int*>    (leaderStrategy);

			Globals::logger.Log(pursuit, "[SUP] Priority: LeaderStrategy", strategyID);
		}
	}



	template <address CountFunction, address RetrievalFunction, auto IsStrategyAvailable>
	requires std::predicate<decltype(IsStrategyAvailable), address, address>
	void MarshalStrategies
	(
		const address         pursuit,
		std::vector<address>& candidates
	) {
		const auto GetSupportNode = reinterpret_cast<address (__thiscall*)(address)>(0x418EE0);

		const address supportNode = GetSupportNode(pursuit - 0x48);
		if (not supportNode) return; // should never happen

		const auto GetNumStrategies = reinterpret_cast<size_t  (__thiscall*)(address)>        (CountFunction);
		const auto GetStrategy      = reinterpret_cast<address (__thiscall*)(address, size_t)>(RetrievalFunction);

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; ++strategyID)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsStrategyAvailable(pursuit, strategy)) continue;

			const int chance = *reinterpret_cast<volatile int*>(strategy + 0x4);

			if (Globals::prng.DoPercentTrial<int>(chance))
				candidates.push_back(strategy);
		}
	}



	void SetStrategy
	(
		const address pursuit,
		const address strategy,
		const bool    isHeavyStrategy
	) {
		const float duration = *reinterpret_cast<volatile float*>(strategy + 0x8);

		*reinterpret_cast<volatile float*>(pursuit + 0x208) = duration; // strategy duration
		*reinterpret_cast<volatile int*>  (pursuit + 0x20C) = 1;        // request flag

		if (isHeavyStrategy)
		{
			const int strategyID = *reinterpret_cast<volatile int*>(strategy);

			if ((strategyID != 3) or heavy3TriggerCooldowns.current)
				*reinterpret_cast<volatile float*>(pursuit + 0xC8) = roadblockHeavyCooldowns.current; // roadblock cooldown

			*reinterpret_cast<volatile address*>(pursuit + 0x194) = strategy; // HeavyStrategy
		}
		else *reinterpret_cast<volatile address*>(pursuit + 0x198) = strategy; // LeaderStrategy
	}



	bool __fastcall SetRandomStrategy(const address pursuit) 
	{
		static constinit std::vector<address> candidates;

		// Marshal all currently eligible Strategies
		const bool isPlayerPursuit = Globals::IsPlayerPursuit(pursuit);

		if (isPlayerPursuit or rivalHeavyEnableds.current)
			MarshalStrategies<0x403600, 0x4035E0, IsHeavyStrategyAvailable>(pursuit, candidates);

		const size_t numHeavyStrategies = candidates.size();

		if (isPlayerPursuit or rivalLeaderEnableds.current)
			MarshalStrategies<0x403680, 0x403660, IsLeaderStrategyAvailable>(pursuit, candidates);

		// Check candidate count
		if (candidates.empty())
		{
			if constexpr (Globals::loggingEnabled)
			{
				const bool canMakeRequest = (isPlayerPursuit or rivalHeavyEnableds.current or rivalLeaderEnableds.current);
				Globals::logger.Log(pursuit, "[SUP] Strategy request failed", (canMakeRequest) ? "(chance)" : "(blocked)");
			}

			return false;
		}

		// Select an eligible Strategy at random
		const size_t index = Globals::prng.GenerateIndex(candidates.size());

		const address randomStrategy  = candidates[index];
		const bool    isHeavyStrategy = (index < numHeavyStrategies);

		SetStrategy(pursuit, randomStrategy, isHeavyStrategy);

		if constexpr (Globals::loggingEnabled)
		{
			const int strategyID = *reinterpret_cast<volatile int*>(randomStrategy);

			Globals::logger.Log<0>(pursuit, "[SUP] Requesting", (isHeavyStrategy) ? "HeavyStrategy" : "LeaderStrategy", strategyID);
			Globals::logger.Log<2>("Candidate", static_cast<int>(index + 1), '/', static_cast<int>(candidates.size()));
		}

		candidates.clear();

		return true;
	}



	int GetGlobalNumPersistentCops()
	{
		if (not Globals::copManager) return 0;

		int numPersistentVehicles = *reinterpret_cast<volatile int*>(Globals::copManager + 0x94); // cops loaded

		const address finalPursuitEntry   = *reinterpret_cast<volatile address*>(Globals::copManager + 0x128);
		address       currentPursuitEntry = *reinterpret_cast<volatile address*>(finalPursuitEntry);

		// Check each active pursuit for roadblock(s)
		while (currentPursuitEntry != finalPursuitEntry)
		{
			const address pursuit   = *reinterpret_cast<volatile address*>(currentPursuitEntry + 0x8);
			const address roadblock = *reinterpret_cast<volatile address*>(pursuit + 0x84);

			// Subtract roadblock-vehicle count
			if (roadblock)
			{
				const address firstVehicleEntry = *reinterpret_cast<volatile address*>(roadblock + 0xC);
				const address lastVehicleEntry  = *reinterpret_cast<volatile address*>(roadblock + 0x10);

				if (lastVehicleEntry > firstVehicleEntry)
					numPersistentVehicles -= (lastVehicleEntry - firstVehicleEntry) / sizeof(address);
			}

			currentPursuitEntry = *reinterpret_cast<volatile address*>(currentPursuitEntry);
		}

		return numPersistentVehicles;
	}



	bool __fastcall HasJoinCapacity(const address pursuit)
	{
		const int numVehiclesJoined = *reinterpret_cast<volatile int*>(pursuit + 0x23C);
		if (numVehiclesJoined >= maxRBJoinCounts.current) return false;

		const float distanceToRoadblock = *reinterpret_cast<volatile float*>(pursuit + 0x7C);
		if (distanceToRoadblock > maxRBJoinDistances.current) return false;

		// Consult ChasersManager for cop capacity (if enabled)
		if (CopSpawnOverrides::featureEnabled)
		{
			if (not CopSpawnOverrides::ChasersManager::HasJoinCapacity(pursuit)) 
				return false;

			else if (CopSpawnOverrides::chasersAreIndependents.current) 
				return true;
		}

		// Struct contains vanilla global cop-spawn limit if CopSpawnOverrides feature is disabled
		return (GetGlobalNumPersistentCops() < CopSpawnOverrides::activeChaserCounts.maxValues.current);
	}



	bool __fastcall MayDetachCops(const address roadblock)
	{
		const address pursuit   = *reinterpret_cast<volatile address*>(roadblock + 0x28);
		const float   joinTimer = *reinterpret_cast<volatile float*>  (roadblock + 0x58);

		switch (*reinterpret_cast<volatile int*>(pursuit + 0x218)) // pursuit status
		{
		case 0: // default pursuit state
			return (regularRBJoinTimers.isEnableds.current and (joinTimer > regularRBJoinTimers.values.current));

		case 1: // active "Backup" timer
			return (backupRBJoinTimers.isEnableds.current and (joinTimer > backupRBJoinTimers.values.current));

		case 2: // "COOLDOWN" mode
			return reactToCooldownModes.current;
		}

		return false;
	}



	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address onAttachedEntrance = 0x424036;
	constexpr address onAttachedExit     = 0x42403C;

	// Updates the Cross flag whenever a Cross replacement joins
	__declspec(naked) void OnAttached()
	{
		__asm
		{
			cmp byte ptr LeaderOverrides::featureEnabled, 0x1
			je conclusion // flag managed by "Advanced" feature set

			mov edx, dword ptr [edi + 0x54]       // AIVehicle
			cmp byte ptr [edx - 0x4C + 0x83], 0x1 // padding byte: Cross flag (car)
			jne conclusion                        // not Cross' vehicle

			mov dword ptr [esi + 0x174], 0x1 // Cross flag (pursuit)
			
			conclusion:
			// Execute original code and resume
			fild dword ptr [esi + 0x148]

			jmp dword ptr onAttachedExit
		}
	}



	constexpr address onDetachedEntrance = 0x42B5F1;
	constexpr address onDetachedExit     = 0x42B616;

	// Checks whether a despawning vehicle was a Cross replacement
	__declspec(naked) void OnDetached()
	{
		__asm
		{
			// Execute original code first
			mov ecx, esi
			mov edx, dword ptr [esi]

			mov eax, dword ptr [esi + 0x54]       // AIVehicle
			cmp byte ptr [eax - 0x4C + 0x83], 0x1 // padding byte: Cross flag (car)

			jmp dword ptr onDetachedExit
		}
	}



	constexpr address crossSpawnEntrance = 0x41F7D9;
	constexpr address crossSpawnExit     = 0x41F7E0;

	// Marks Cross' replacement for later identification
	__declspec(naked) void CrossSpawn()
	{
		__asm
		{
			mov eax, dword ptr [esp + 0x94] // final Strategy vehicle
			mov edx, dword ptr [eax + 0xC]  // current Strategy vehicle

			cmp ebp, edx
			sete byte ptr [ebx - 0x4C + 0x83] // padding byte: Cross flag (car)

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x90]

			jmp dword ptr crossSpawnExit
		}
	}



	constexpr address henchmenSubEntrance = 0x41F485;
	constexpr address henchmenSubExit     = 0x41F497;

	// Replaces henchmen vehicles
	__declspec(naked) void HenchmenSub()
	{
		__asm
		{
			push esi

			mov eax, dword ptr leader7Hench1Vehicles.current
			mov edx, dword ptr leader7Hench2Vehicles.current

			// Only LeaderStrategy 7 reads these
			mov dword ptr [esp + 0x24], eax
			mov dword ptr [esp + 0x28], edx

			mov esi, dword ptr [esp + 0x60]

			jmp dword ptr henchmenSubExit
		}
	}



	constexpr address heavySelectorEntrance = 0x41F1A4;
	constexpr address heavySelectorExit     = 0x41F1C8;

	// Replaces HeavyStrategy vehicles
	__declspec(naked) void HeavySelector()
	{
		__asm
		{
			mov ecx, dword ptr [esi]
			call SelectHeavyVehicle // ecx: heavyStrategy

			jmp dword ptr heavySelectorExit
		}
	}



	constexpr address crossSelectorEntrance = 0x41F504;
	constexpr address crossSelectorExit     = 0x41F50C;

	// Replaces Cross' vehicle
	__declspec(naked) void CrossSelector()
	{
		__asm
		{
			mov edi, eax

			mov ecx, dword ptr [esi + 0x4]
			call SelectCrossVehicle // ecx: LeaderStrategy
			mov dword ptr [esp + 0x24], eax

			test edi, edi
			mov eax, edi

			jmp dword ptr crossSelectorExit
		}
	}



	constexpr address crossPriorityEntrance = 0x419724;
	constexpr address crossPriorityExit     = 0x41972A;

	// Can skip the Cross priority request in rival pursuits
	__declspec(naked) void CrossPriority()
	{
		static constexpr address crossPrioritySkip = 0x419780;

		__asm
		{
			jne skip // priority flag set

			cmp byte ptr rivalLeaderEnableds.current, 0x1
			je conclusion // no rival discrimination

			mov ecx, esi
			call Globals::IsPlayerPursuit
			test al, al
			je skip // not player pursuit

			conclusion:
			// Execute original code and resume
			mov ecx, ebp
			xor ebx, ebx

			jmp dword ptr crossPriorityExit

			skip:
			jmp dword ptr crossPrioritySkip
		}
	}



	constexpr address rivalRoadblockEntrance = 0x419563;
	constexpr address rivalRoadblockExit     = 0x419568;

	// Can skip roadblock requests in rival pursuits
	__declspec(naked) void RivalRoadblock()
	{
		static constexpr address rivalRoadblockSkip = 0x4195CD;

		__asm
		{
			cmp byte ptr rivalRoadblockEnableds.current, 0x1
			je conclusion // no rival discrimination

			call Globals::IsPlayerPursuit
			test al, al
			je skip // not player pursuit

			mov ecx, esi
			mov edx, dword ptr [esi]

			conclusion:
			// Execute original code and resume
			call dword ptr [edx + 0x28] // AIPursuit::IsPerpInSight
			cmp al, 0x1

			jmp dword ptr rivalRoadblockExit

			skip:
			jmp dword ptr rivalRoadblockSkip
		}
	}



	constexpr address priorityOutcomeEntrance = 0x419770;
	constexpr address priorityOutcomeExit     = 0x419776;

	// Reports the outcome of LeaderStrategy priority
	__declspec(naked) void PriorityOutcome()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0x208], eax

			mov ecx, esi
			call ReportPriorityOutcome // ecx: pursuit

			jmp dword ptr priorityOutcomeExit
		}
	}



	constexpr address requestCooldownEntrance = 0x4196D7;
	constexpr address requestCooldownExit     = 0x4196E4;

	// Updates the Strategy cooldown after each request
	__declspec(naked) void RequestCooldown()
	{
		__asm
		{
			mov ecx, offset strategyCooldowns
			call HeatParameters::Interval<float>::GetRandomValue
			fstp dword ptr [esi + 0x210] // strategy cooldown

			// Execute original code and resume
			lea ecx, dword ptr [esi - 0x48]

			jmp dword ptr requestCooldownExit
		}
	}



	constexpr address roadblockCooldownEntrance = 0x419535;
	constexpr address roadblockCooldownExit     = 0x41954C;

	// Updates the non-Strategy roadblock cooldown after each request
	__declspec(naked) void RoadblockCooldown()
	{
		__asm
		{
			mov ecx, offset roadblockCooldowns
			call HeatParameters::Interval<float>::GetRandomValue

			jmp dword ptr roadblockCooldownExit
		}
	}



	constexpr address roadblockDistanceEntrance = 0x43DE45;
	constexpr address roadblockDistanceExit     = 0x43DE4A;

	// Changes the distance at which roadblocks can spawn from racers
	__declspec(naked) void RoadblockDistance()
	{
		__asm
		{
			push ecx

			mov ecx, offset roadblockSpawnDistances
			call HeatParameters::Interval<float>::GetRandomValue

			mov ecx, dword ptr [esp]
			fstp dword ptr [esp]

			jmp dword ptr roadblockDistanceExit
		}
	}



	constexpr address strategySelectionEntrance = 0x41978D;
	constexpr address strategySelectionExit     = 0x41984E;

	// Selects a random available Strategy without any biases
	__declspec(naked) void StrategySelection()
	{
		__asm
		{
			mov ecx, esi
			call SetRandomStrategy // ecx: pursuit
			movzx eax, al

			xor edi, edi
			sub edi, eax

			jmp dword ptr strategySelectionExit
		}
	}



	constexpr address spikesHitReactionEntrance = 0x63BB9A;
	constexpr address spikesHitReactionExit     = 0x63BBA6;

	// Can suppress roadblock reactions to spike-strip hits
	__declspec(naked) void SpikesHitReaction()
	{
		static constexpr float maxJoinRange = 80.f; // metres

		__asm
		{
			// Execute original code first
			mov eax, dword ptr [eax + 0x70] // roadblock pursuit
			test eax, eax
			je conclusion                   // no pursuit

			cmp byte ptr reactToSpikesHits.current, 0x0
			je conclusion // reaction disabled

			mov edx, eax

			fld dword ptr [edx + 0x7C] // distance to racer
			fcomp dword ptr maxJoinRange
			fnstsw ax
			test ah, 0x41

			mov eax, edx

			conclusion:
			jmp dword ptr spikesHitReactionExit
		}
	}



	constexpr address roadblockFormationEntrance = 0x40AE5A;
	constexpr address roadblockFormationExit     = 0x40AE63;

	// Can prevent roadblock spawns from cancelling cop formations
	__declspec(naked) void RoadblockFormation()
	{
		__asm
		{
			cmp byte ptr roadblockEndsFormations.current, 0x1
			jne conclusion // keep formation

			// Execute original code and resume
			mov eax, dword ptr [esi]
			mov ecx, esi
			call dword ptr [eax + 0x58] // AIPursuit::IsFinisherActive
			test al, al

			conclusion:
			jmp dword ptr roadblockFormationExit
		}
	}



	constexpr address roadblockJoinCountEntrance = 0x4443A6;
	constexpr address roadblockJoinCountExit     = 0x4443AE;

	// Enforces the join limit for roadblock vehicles
	__declspec(naked) void RoadblockJoinCount()
	{
		static constexpr address roadblockJoinCountSkip = 0x444400;

		__asm
		{
			lea ecx, dword ptr [esi + 0x40]
			call HasJoinCapacity // ecx: pursuit
			test al, al
			je skip              // may not join

			jmp dword ptr roadblockJoinCountExit

			skip:
			jmp dword ptr roadblockJoinCountSkip
		}
	}



	constexpr address roadblockJoinTimerEntrance = 0x42BF06;
	constexpr address roadblockJoinTimerExit     = 0x42BF2B;

	// Checks the timer for joining from roadblocks
	__declspec(naked) void RoadblockJoinTimer()
	{
		__asm
		{
			fstp dword ptr [ebp + 0x58] // join timer

			mov ecx, ebp
			call MayDetachCops // ecx: roadblock
			cmp al, 0x1

			jmp dword ptr roadblockJoinTimerExit
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ValidateVehicleTypes()
	{
		bool allValid = true;

		allValid &= HeatParameters::ValidateVehicleTypes("Heavy 3, light", heavy3LightVehicles, Globals::IsVehicleTypeCar);
		allValid &= HeatParameters::ValidateVehicleTypes("Heavy 3, heavy", heavy3HeavyVehicles, Globals::IsVehicleTypeCar);
		allValid &= HeatParameters::ValidateVehicleTypes("Heavy 4, light", heavy4LightVehicles, Globals::IsVehicleTypeCar);
		allValid &= HeatParameters::ValidateVehicleTypes("Heavy 4, heavy", heavy4HeavyVehicles, Globals::IsVehicleTypeCar);

		allValid &= HeatParameters::ValidateVehicleTypes("Leader 5, Cross",   leader5CrossVehicles,  Globals::IsVehicleTypeCar);
		allValid &= HeatParameters::ValidateVehicleTypes("Leader 7, Cross",   leader7CrossVehicles,  Globals::IsVehicleTypeCar);
		allValid &= HeatParameters::ValidateVehicleTypes("Leader 7, hench 1", leader7Hench1Vehicles, Globals::IsVehicleTypeCar);
		allValid &= HeatParameters::ValidateVehicleTypes("Leader 7, hench 2", leader7Hench2Vehicles, Globals::IsVehicleTypeCar);

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.Log<2>("All vehicles valid");
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Also fixes the unintended biases in the Strategy-selection process
		MemoryTools::MakeRangeJMP<strategySelectionEntrance, strategySelectionExit>(StrategySelection);

		// Also prevents excessive joining from roadblocks
		MemoryTools::MakeRangeJMP<roadblockJoinCountEntrance, roadblockJoinCountExit>(RoadblockJoinCount);
	}



    bool Initialise(HeatParameters::Parser& parser)
    {
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupports");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Supports.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Supports:Rivals", rivalRoadblockEnableds, rivalHeavyEnableds, rivalLeaderEnableds);

		HeatParameters::Parse(parser, "Roadblocks:Cooldown",   roadblockCooldowns,       roadblockHeavyCooldowns);
		HeatParameters::Parse(parser, "Roadblocks:Distance",   roadblockSpawnDistances);
		HeatParameters::Parse(parser, "Roadblocks:Formations", roadblockEndsFormations);
		HeatParameters::Parse(parser, "Roadblocks:Joining",    regularRBJoinTimers,      backupRBJoinTimers);
		HeatParameters::Parse(parser, "Roadblocks:Reactions",  reactToCooldownModes,     reactToSpikesHits);
		HeatParameters::Parse(parser, "Joining:Definitions",   maxRBJoinDistances,       maxRBJoinElevationDeltas, maxRBJoinCounts);

		HeatParameters::Parse(parser, "Strategies:Cooldown", strategyCooldowns);
		HeatParameters::Parse(parser, "Heavy3:Roadblocks",   heavy3TriggerCooldowns, heavy3AreBlockables);
		HeatParameters::Parse(parser, "Heavy3:Vehicles",     heavy3LightVehicles,    heavy3HeavyVehicles);
		HeatParameters::Parse(parser, "Heavy4:Vehicles",     heavy4LightVehicles,    heavy4HeavyVehicles);
		HeatParameters::Parse(parser, "Leader5:Vehicle",     leader5CrossVehicles);
		HeatParameters::Parse(parser, "Leader7:Vehicles",    leader7CrossVehicles,   leader7Hench1Vehicles, leader7Hench2Vehicles);

		// Check whether vehicles are cars
		ValidateVehicleTypes();

		// Code modifications (feature-specific)
		if constexpr (Globals::loggingEnabled)
			MemoryTools::MakeRangeJMP<priorityOutcomeEntrance, priorityOutcomeExit>(PriorityOutcome);

		// Code modifications (geneal)
		MemoryTools::Write<float*>(&(maxRBJoinDistances.current),       {0x42BEBC});
		MemoryTools::Write<float*>(&(maxRBJoinElevationDeltas.current), {0x42BE3A});

		MemoryTools::MakeRangeNOP<0x42BEB6, 0x42BEBA>(); // roadblock-joining flag reset
		MemoryTools::MakeRangeNOP<0x42402A, 0x424036>(); // Cross flag = 1

		MemoryTools::MakeRangeJMP<onAttachedEntrance,         onAttachedExit>        (OnAttached);
		MemoryTools::MakeRangeJMP<onDetachedEntrance,         onDetachedExit>        (OnDetached);
		MemoryTools::MakeRangeJMP<crossSpawnEntrance,         crossSpawnExit>        (CrossSpawn);
		MemoryTools::MakeRangeJMP<henchmenSubEntrance,        henchmenSubExit>       (HenchmenSub);
		MemoryTools::MakeRangeJMP<heavySelectorEntrance,      heavySelectorExit>     (HeavySelector);
		MemoryTools::MakeRangeJMP<crossSelectorEntrance,      crossSelectorExit>     (CrossSelector);
		MemoryTools::MakeRangeJMP<crossPriorityEntrance,      crossPriorityExit>     (CrossPriority);
		MemoryTools::MakeRangeJMP<rivalRoadblockEntrance,     rivalRoadblockExit>    (RivalRoadblock);
		MemoryTools::MakeRangeJMP<requestCooldownEntrance,    requestCooldownExit>   (RequestCooldown);
		MemoryTools::MakeRangeJMP<roadblockCooldownEntrance,  roadblockCooldownExit> (RoadblockCooldown);
		MemoryTools::MakeRangeJMP<roadblockDistanceEntrance,  roadblockDistanceExit> (RoadblockDistance);
		MemoryTools::MakeRangeJMP<spikesHitReactionEntrance,  spikesHitReactionExit> (SpikesHitReaction);
		MemoryTools::MakeRangeJMP<roadblockFormationEntrance, roadblockFormationExit>(RoadblockFormation);
		MemoryTools::MakeRangeJMP<roadblockJoinTimerEntrance, roadblockJoinTimerExit>(RoadblockJoinTimer);
        
		ApplyFixes(); // also contains Strategy-selection and roadblock-joining features
        
		// Status flag
		featureEnabled = true;

		return true;
    }



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [SUP] GroundSupport");

		rivalRoadblockEnableds.Log("rivalRoadblockEnabled   ");
		rivalHeavyEnableds    .Log("rivalHeavyEnabled       ");
		rivalLeaderEnableds   .Log("rivalLeaderEnabled      ");

		roadblockCooldowns     .Log("roadblockCooldown       ");
		roadblockHeavyCooldowns.Log("roadblockHeavyCooldown  ");

		roadblockSpawnDistances.Log("roadblockSpawnDistance  ");
		roadblockEndsFormations.Log("roadblockEndsFormation  ");

		if (regularRBJoinTimers.isEnableds.current or backupRBJoinTimers.isEnableds.current or reactToCooldownModes.current)
		{
			maxRBJoinDistances      .Log("maxRBJoinDistance       ");
			maxRBJoinElevationDeltas.Log("maxRBJoinElevationDeltas");
			maxRBJoinCounts         .Log("maxRBJoinCount          ");
		}

		regularRBJoinTimers.Log("regularRBJoinTimer      ");
		backupRBJoinTimers .Log("backupRBJoinTimer       ");

		reactToCooldownModes.Log("reactToCooldownMode     ");
		reactToSpikesHits   .Log("reactToSpikesHit        ");

		strategyCooldowns.Log("strategyCooldown        ");

		heavy3TriggerCooldowns.Log("heavy3TriggerCooldown   ");
		heavy3AreBlockables   .Log("heavy3AreBlockable      ");

		heavy3LightVehicles.Log("heavy3LightVehicle      ");
		heavy3HeavyVehicles.Log("heavy3HeavyVehicle      ");
		heavy4LightVehicles.Log("heavy4LightVehicle      ");
		heavy4HeavyVehicles.Log("heavy4HeavyVehicle      ");

		leader5CrossVehicles .Log("leader5CrossVehicle     ");
		leader7CrossVehicles .Log("leader7CrossVehicle     ");
		leader7Hench1Vehicles.Log("leader7Hench1Vehicle    ");
		leader7Hench2Vehicles.Log("leader7Hench2Vehicle    ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		rivalRoadblockEnableds.SetToHeat(isRacing, heatLevel);
		rivalHeavyEnableds    .SetToHeat(isRacing, heatLevel);
		rivalLeaderEnableds   .SetToHeat(isRacing, heatLevel);

		roadblockCooldowns     .SetToHeat(isRacing, heatLevel);
		roadblockHeavyCooldowns.SetToHeat(isRacing, heatLevel);

		roadblockSpawnDistances.SetToHeat(isRacing, heatLevel);
		roadblockEndsFormations.SetToHeat(isRacing, heatLevel);

		maxRBJoinDistances      .SetToHeat(isRacing, heatLevel);
		maxRBJoinElevationDeltas.SetToHeat(isRacing, heatLevel);
		maxRBJoinCounts         .SetToHeat(isRacing, heatLevel);

		regularRBJoinTimers.SetToHeat(isRacing, heatLevel);
		backupRBJoinTimers .SetToHeat(isRacing, heatLevel);

		reactToCooldownModes.SetToHeat(isRacing, heatLevel);
		reactToSpikesHits   .SetToHeat(isRacing, heatLevel);

		strategyCooldowns.SetToHeat(isRacing, heatLevel);

		heavy3TriggerCooldowns.SetToHeat(isRacing, heatLevel);
		heavy3AreBlockables   .SetToHeat(isRacing, heatLevel);

		heavy3LightVehicles.SetToHeat(isRacing, heatLevel);
		heavy3HeavyVehicles.SetToHeat(isRacing, heatLevel);
		heavy4LightVehicles.SetToHeat(isRacing, heatLevel);
		heavy4HeavyVehicles.SetToHeat(isRacing, heatLevel);

		leader5CrossVehicles .SetToHeat(isRacing, heatLevel);
		leader7CrossVehicles .SetToHeat(isRacing, heatLevel);
		leader7Hench1Vehicles.SetToHeat(isRacing, heatLevel);
		leader7Hench2Vehicles.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
    }
}