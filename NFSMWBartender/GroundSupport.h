#pragma once

#include <vector>
#include <concepts>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "CopSpawnOverrides.h"
#include "LeaderOverrides.h"



namespace GroundSuppport
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Value<bool> rivalRoadblockEnabled(true);
	constinit HeatParameters::Value<bool> rivalHeavyEnabled    (true);
	constinit HeatParameters::Value<bool> rivalLeaderEnabled   (true);

	constinit HeatParameters::Interval<float> roadblockCooldown     (8.f,  12.f, {1.f}); // seconds
	constinit HeatParameters::Value   <float> roadblockHeavyCooldown(15.f, {1.f});       // seconds

	constinit HeatParameters::Interval<float> roadblockSpawnDistance(250.f, 250.f, {0.f, 400.f}); // metres

	constinit HeatParameters::Value<bool> roadblockEndsFormation(true);

	constinit HeatParameters::OptionalValue<float> regularRBJoinTimer({0.f}); // seconds
	constinit HeatParameters::OptionalValue<float> backupRBJoinTimer ({0.f}); // seconds

	constinit HeatParameters::Value<float> maxRBJoinDistance      (500.f, {0.f}); // metres
	constinit HeatParameters::Value<float> maxRBJoinElevationDelta(1.5f,  {0.f}); // metres
	constinit HeatParameters::Value<int>   maxRBJoinCount         (1,     {0});   // cars

	constinit HeatParameters::Value<bool> reactToCooldownMode(true);
	constinit HeatParameters::Value<bool> reactToSpikesHit   (true);
	
	constinit HeatParameters::Interval<float> strategyCooldown(10.f, 10.f, {1.f}); // seconds

	constinit HeatParameters::Value<float> heavy3SpeedLimit(100.f, {0.f}); // kph

	constinit HeatParameters::Value<bool> heavy3TriggerCooldown(true);
	constinit HeatParameters::Value<bool> heavy3AreBlockable   (true);

	constinit HeatParameters::Value<const char*> heavy3LightVehicle("copsuvl");
	constinit HeatParameters::Value<const char*> heavy3HeavyVehicle("copsuv");

	constinit HeatParameters::Value<const char*> heavy4LightVehicle("copsuvl");
	constinit HeatParameters::Value<const char*> heavy4HeavyVehicle("copsuv");

	constinit HeatParameters::Value<const char*> leader5CrossVehicle("copcross");

	constinit HeatParameters::Value<const char*> leader7CrossVehicle ("copcross");
	constinit HeatParameters::Value<const char*> leader7Hench1Vehicle("copsporthench");
	constinit HeatParameters::Value<const char*> leader7Hench2Vehicle("copsporthench");

	// Conversions
	float rammingSpeedLimit = heavy3SpeedLimit.current / 3.6f; // mps





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const char* __fastcall SelectHeavyVehicle(const address heavyStrategy)
	{
		const int strategyID = AsVolatile<int>(heavyStrategy);

		const int  heavyChance = AsVolatile<int>(heavyStrategy + 0xC);
		const bool isHeavy     = Globals::prng.DoPercentTrial<int>(heavyChance);

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
		const int strategyID = AsVolatile<int>(leaderStrategy);

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
		const int  strategyID   = AsVolatile<int>    (heavyStrategy);
		const bool hasRoadblock = AsVolatile<address>(pursuit + 0x84);

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
		const int crossFlag = AsVolatile<int>(pursuit + 0x164);

		if (crossFlag == 0)
		{
			const int strategyID = AsVolatile<int>(leaderStrategy);

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
			const address leaderStrategy = AsVolatile<address>(pursuit + 0x198);
			const int     strategyID     = AsVolatile<int>    (leaderStrategy);

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
		const auto GetSupportNode = AsFunction<address __thiscall (address)>(0x418EE0);

		const address supportNode = GetSupportNode(pursuit - 0x48);
		if (not supportNode) return; // should never happen

		const auto GetNumStrategies = AsFunction<size_t  __thiscall (address)>        (CountFunction);
		const auto GetStrategy      = AsFunction<address __thiscall (address, size_t)>(RetrievalFunction);

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; ++strategyID)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsStrategyAvailable(pursuit, strategy)) continue;

			const int chance = AsVolatile<int>(strategy + 0x4);

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
		const float duration = AsVolatile<float>(strategy + 0x8);

		AsVolatile<float>(pursuit + 0x208) = duration; // strategy duration
		AsVolatile<int>  (pursuit + 0x20C) = 1;        // request flag

		if (isHeavyStrategy)
		{
			const int strategyID = AsVolatile<int>(strategy);

			if ((strategyID != 3) or heavy3TriggerCooldown.current)
				AsVolatile<float>(pursuit + 0xC8) = roadblockHeavyCooldown.current; // roadblock cooldown

			AsVolatile<address>(pursuit + 0x194) = strategy; // HeavyStrategy
		}
		else AsVolatile<address>(pursuit + 0x198) = strategy; // LeaderStrategy
	}



	bool __fastcall SetRandomStrategy(const address pursuit) 
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
				Globals::logger.Log(pursuit, "[SUP] Strategy request failed", (canMakeRequest) ? "(chance)" : "(blocked)");
			}

			return false;
		}

		// Select an eligible Strategy at random
		const size_t  randomIndex     = Globals::prng.GenerateIndex(candidates.size());
		const address randomStrategy  = candidates[randomIndex];
		const bool    isHeavyStrategy = (randomIndex < numHeavyStrategies);

		SetStrategy(pursuit, randomStrategy, isHeavyStrategy);

		if constexpr (Globals::loggingEnabled)
		{
			const int strategyID = AsVolatile<int>(randomStrategy);

			Globals::logger.Log<0>(pursuit, "[SUP] Requesting", (isHeavyStrategy) ? "HeavyStrategy" : "LeaderStrategy", strategyID);
			Globals::logger.Log<2>("Candidate", DecFormat(randomIndex + 1), '/', DecFormat(candidates.size()));
		}

		candidates.clear();

		return true;
	}



	[[nodiscard]] int GetGlobalNumPersistentCops()
	{
		if (not Globals::copManager) return 0;

		int numPersistentVehicles = AsVolatile<int>(Globals::copManager + 0x94); // cops loaded

		const address finalPursuitEntry   = AsVolatile<address>(Globals::copManager + 0x128);
		address       currentPursuitEntry = AsVolatile<address>(finalPursuitEntry);

		// Check each active pursuit for roadblock(s)
		while (currentPursuitEntry != finalPursuitEntry)
		{
			const address pursuit   = AsVolatile<address>(currentPursuitEntry + 0x8);
			const address roadblock = AsVolatile<address>(pursuit + 0x84);

			// Subtract roadblock-vehicle count
			if (roadblock)
			{
				const address firstVehicleEntry = AsVolatile<address>(roadblock + 0xC);
				const address lastVehicleEntry  = AsVolatile<address>(roadblock + 0x10);

				if (lastVehicleEntry > firstVehicleEntry)
					numPersistentVehicles -= (lastVehicleEntry - firstVehicleEntry) / sizeof(address);
			}

			currentPursuitEntry = AsVolatile<address>(currentPursuitEntry);
		}

		return numPersistentVehicles;
	}



	[[nodiscard]] bool __fastcall HasJoinCapacity(const address pursuit)
	{
		const int numVehiclesJoined = AsVolatile<int>(pursuit + 0x23C);
		if (numVehiclesJoined >= maxRBJoinCount.current) return false;

		const float distanceToRoadblock = AsVolatile<float>(pursuit + 0x7C);
		if (distanceToRoadblock > maxRBJoinDistance.current) return false;

		// Consult ChasersManager for cop capacity (if enabled)
		if (CopSpawnOverrides::anyFeatureEnabled)
		{
			if (not CopSpawnOverrides::ChasersManager::HasJoinCapacity(pursuit)) 
				return false;

			else if (CopSpawnOverrides::chasersAreIndependent.current) 
				return true;
		}

		// Struct contains vanilla global cop-spawn limit if CopSpawnOverrides feature is disabled
		return (GetGlobalNumPersistentCops() < CopSpawnOverrides::activeChaserCount.max.current);
	}



	[[nodiscard]] bool __fastcall MayDetachCops(const address roadblock)
	{
		const address pursuit       = AsVolatile<address>(roadblock + 0x28);
		const int     pursuitStatus = AsVolatile<int>    (pursuit   + 0x218);

		const float joinTimer = AsVolatile<float>(roadblock + 0x58);

		switch (pursuitStatus)
		{
		case 0: // default pursuit state
			return (regularRBJoinTimer.isEnabled.current and (joinTimer > regularRBJoinTimer.value.current));

		case 1: // active "Backup" timer
			return (backupRBJoinTimer.isEnabled.current and (joinTimer > backupRBJoinTimer.value.current));

		case 2: // "COOLDOWN" mode
			return reactToCooldownMode.current;
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
			cmp byte ptr [LeaderOverrides::anyFeatureEnabled], 1
			je conclusion // flag managed by "Advanced" feature set

			mov edx, dword ptr [edi + 0x54]     // AIVehicle
			cmp byte ptr [edx - 0x4C + 0x83], 1 // padding byte: Cross flag (car)
			jne conclusion                      // not Cross' vehicle

			mov dword ptr [esi + 0x174], 1 // Cross flag (pursuit)
			
			conclusion:
			// Execute original code and resume
			fild dword ptr [esi + 0x148]

			jmp dword ptr [onAttachedExit]
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

			mov eax, dword ptr [esi + 0x54]     // AIVehicle
			cmp byte ptr [eax - 0x4C + 0x83], 1 // padding byte: Cross flag (car)

			jmp dword ptr [onDetachedExit]
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

			jmp dword ptr [crossSpawnExit]
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

			mov eax, dword ptr [leader7Hench1Vehicle.current]
			mov edx, dword ptr [leader7Hench2Vehicle.current]

			// Only LeaderStrategy 7 reads these
			mov dword ptr [esp + 0x24], eax
			mov dword ptr [esp + 0x28], edx

			mov esi, dword ptr [esp + 0x60]

			jmp dword ptr [henchmenSubExit]
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

			jmp dword ptr [heavySelectorExit]
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

			jmp dword ptr [crossSelectorExit]
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

			cmp byte ptr [rivalLeaderEnabled.current], 1
			je conclusion // no rival discrimination

			mov ecx, esi
			call Globals::IsPlayerPursuit
			test al, al
			je skip // not player pursuit

			conclusion:
			// Execute original code and resume
			mov ecx, ebp
			xor ebx, ebx

			jmp dword ptr [crossPriorityExit]

			skip:
			jmp dword ptr [crossPrioritySkip]
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
			cmp byte ptr [rivalRoadblockEnabled.current], 1
			je conclusion // no rival discrimination

			call Globals::IsPlayerPursuit
			test al, al
			je skip // not player pursuit

			mov ecx, esi
			mov edx, dword ptr [esi]

			conclusion:
			// Execute original code and resume
			call dword ptr [edx + 0x28] // AIPursuit::IsPerpInSight
			cmp al, 1

			jmp dword ptr [rivalRoadblockExit]

			skip:
			jmp dword ptr [rivalRoadblockSkip]
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

			jmp dword ptr [priorityOutcomeExit]
		}
	}



	constexpr address requestCooldownEntrance = 0x4196D7;
	constexpr address requestCooldownExit     = 0x4196E4;

	// Updates the Strategy cooldown after each request
	__declspec(naked) void RequestCooldown()
	{
		__asm
		{
			mov ecx, offset strategyCooldown
			call HeatParameters::Interval<float>::GetRandomValue
			fstp dword ptr [esi + 0x210] // strategy cooldown

			// Execute original code and resume
			lea ecx, dword ptr [esi - 0x48]

			jmp dword ptr [requestCooldownExit]
		}
	}



	constexpr address heavySpeedSetupEntrance = 0x421526;
	constexpr address heavySpeedSetupExit     = 0x421545;

	// Sets the initial speed of HeavyStrategy 3 vehicles
	__declspec(naked) void HeavySpeedSetup()
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

			jmp dword ptr [heavySpeedSetupExit]
		}
	}



	constexpr address heavySpeedUpdateEntrance = 0x4215F6;
	constexpr address heavySpeedUpdateExit     = 0x4215FB;

	// Updates the speed of HeavyStrategy 3 vehicles
	__declspec(naked) void HeavySpeedUpdate()
	{
		__asm
		{
			push dword ptr [rammingSpeedLimit]

			jmp dword ptr [heavySpeedUpdateExit]
		}
	}



	constexpr address roadblockCooldownEntrance = 0x419535;
	constexpr address roadblockCooldownExit     = 0x41954C;

	// Updates the non-Strategy roadblock cooldown after each request
	__declspec(naked) void RoadblockCooldown()
	{
		__asm
		{
			mov ecx, offset roadblockCooldown
			call HeatParameters::Interval<float>::GetRandomValue

			jmp dword ptr [roadblockCooldownExit]
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

			mov ecx, offset roadblockSpawnDistance
			call HeatParameters::Interval<float>::GetRandomValue

			mov ecx, dword ptr [esp]
			fstp dword ptr [esp]

			jmp dword ptr [roadblockDistanceExit]
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

			jmp dword ptr [strategySelectionExit]
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

			cmp byte ptr [reactToSpikesHit.current], 0
			je conclusion // reaction disabled

			mov edx, eax

			fld dword ptr [edx + 0x7C] // distance to target
			fcomp dword ptr [maxJoinRange]
			fnstsw ax
			test ah, 0x41

			mov eax, edx

			conclusion:
			jmp dword ptr [spikesHitReactionExit]
		}
	}



	constexpr address roadblockFormationEntrance = 0x40AE5A;
	constexpr address roadblockFormationExit     = 0x40AE63;

	// Can prevent roadblock spawns from cancelling cop formations
	__declspec(naked) void RoadblockFormation()
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
			jmp dword ptr [roadblockFormationExit]
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

			jmp dword ptr [roadblockJoinCountExit]

			skip:
			jmp dword ptr [roadblockJoinCountSkip]
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
			cmp al, 1

			jmp dword ptr [roadblockJoinTimerExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ResolveAllVehicleNames()
	{
		bool allTypesValid = true;

		const auto Validate = [&allTypesValid](const std::string_view ValueName, auto& vehicleValue) -> void
		{
			allTypesValid &= HeatParameters::ResolveVehicleNames(ValueName, vehicleValue, Globals::IsVehicleTypeCar);
		};

		Validate("Heavy 3, light", heavy3LightVehicle);
		Validate("Heavy 3, heavy", heavy3HeavyVehicle);

		Validate("Heavy 4, light", heavy4LightVehicle);
		Validate("Heavy 4, heavy", heavy4HeavyVehicle);

		Validate("Leader 5, Cross", leader5CrossVehicle);

		Validate("Leader 7, Cross",   leader7CrossVehicle);
		Validate("Leader 7, hench 1", leader7Hench1Vehicle);
		Validate("Leader 7, hench 2", leader7Hench2Vehicle);

		if constexpr (Globals::loggingEnabled)
		{
			if (allTypesValid)
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



	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupport");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Support.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Support:Rivals", rivalRoadblockEnabled, rivalHeavyEnabled, rivalLeaderEnabled);

		HeatParameters::Parse(parser, "Roadblocks:Cooldown",   roadblockCooldown,       roadblockHeavyCooldown);
		HeatParameters::Parse(parser, "Roadblocks:Distance",   roadblockSpawnDistance);
		HeatParameters::Parse(parser, "Roadblocks:Formations", roadblockEndsFormation);
		HeatParameters::Parse(parser, "Roadblocks:Joining",    regularRBJoinTimer,      backupRBJoinTimer);
		HeatParameters::Parse(parser, "Roadblocks:Reactions",  reactToCooldownMode,     reactToSpikesHit);
		HeatParameters::Parse(parser, "Joining:Definitions",   maxRBJoinDistance,       maxRBJoinElevationDelta, maxRBJoinCount);

		HeatParameters::Parse(parser, "Strategies:Cooldown", strategyCooldown);
		HeatParameters::Parse(parser, "Heavy3:Speed",        heavy3SpeedLimit);
		HeatParameters::Parse(parser, "Heavy3:Roadblocks",   heavy3TriggerCooldown, heavy3AreBlockable);
		HeatParameters::Parse(parser, "Heavy3:Vehicles",     heavy3LightVehicle,    heavy3HeavyVehicle);
		HeatParameters::Parse(parser, "Heavy4:Vehicles",     heavy4LightVehicle,    heavy4HeavyVehicle);
		HeatParameters::Parse(parser, "Leader5:Vehicle",     leader5CrossVehicle);
		HeatParameters::Parse(parser, "Leader7:Vehicles",    leader7CrossVehicle,   leader7Hench1Vehicle, leader7Hench2Vehicle);

		// Check and make vehicle names persistent
		ResolveAllVehicleNames();

		// Code modifications (conditional)
		if constexpr (Globals::loggingEnabled)
			MemoryTools::MakeRangeJMP<priorityOutcomeEntrance, priorityOutcomeExit>(PriorityOutcome);

		// Code modifications (geneal)
		MemoryTools::Write<float*>(&(maxRBJoinDistance.current),       {0x42BEBC});
		MemoryTools::Write<float*>(&(maxRBJoinElevationDelta.current), {0x42BE3A});

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
		MemoryTools::MakeRangeJMP<heavySpeedSetupEntrance,    heavySpeedSetupExit>   (HeavySpeedSetup);
		MemoryTools::MakeRangeJMP<heavySpeedUpdateEntrance,   heavySpeedUpdateExit>  (HeavySpeedUpdate);
		MemoryTools::MakeRangeJMP<roadblockCooldownEntrance,  roadblockCooldownExit> (RoadblockCooldown);
		MemoryTools::MakeRangeJMP<roadblockDistanceEntrance,  roadblockDistanceExit> (RoadblockDistance);
		MemoryTools::MakeRangeJMP<spikesHitReactionEntrance,  spikesHitReactionExit> (SpikesHitReaction);
		MemoryTools::MakeRangeJMP<roadblockFormationEntrance, roadblockFormationExit>(RoadblockFormation);
		MemoryTools::MakeRangeJMP<roadblockJoinTimerEntrance, roadblockJoinTimerExit>(RoadblockJoinTimer);

		ApplyFixes(); // also contains Strategy-selection and roadblock-joining features
		
		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		Globals::logger.Log("    HEAT [SUP] GroundSupport");

		rivalRoadblockEnabled.Log("rivalRoadblockEnabled   ");
		rivalHeavyEnabled    .Log("rivalHeavyEnabled       ");
		rivalLeaderEnabled   .Log("rivalLeaderEnabled      ");

		roadblockCooldown     .Log("roadblockCooldown       ");
		roadblockHeavyCooldown.Log("roadblockHeavyCooldown  ");

		roadblockSpawnDistance.Log("roadblockSpawnDistance  ");
		roadblockEndsFormation.Log("roadblockEndsFormation  ");

		if (regularRBJoinTimer.isEnabled.current or backupRBJoinTimer.isEnabled.current or reactToCooldownMode.current)
		{
			maxRBJoinDistance      .Log("maxRBJoinDistance       ");
			maxRBJoinElevationDelta.Log("maxRBJoinElevationDeltas");
			maxRBJoinCount         .Log("maxRBJoinCount          ");
		}

		regularRBJoinTimer.Log("regularRBJoinTimer      ");
		backupRBJoinTimer .Log("backupRBJoinTimer       ");

		reactToCooldownMode.Log("reactToCooldownMode     ");
		reactToSpikesHit   .Log("reactToSpikesHit        ");

		strategyCooldown.Log("strategyCooldown        ");

		heavy3SpeedLimit.Log("heavy3SpeedLimit        ");

		heavy3TriggerCooldown.Log("heavy3TriggerCooldown   ");
		heavy3AreBlockable   .Log("heavy3AreBlockable      ");

		heavy3LightVehicle.Log("heavy3LightVehicle      ");
		heavy3HeavyVehicle.Log("heavy3HeavyVehicle      ");

		heavy4LightVehicle.Log("heavy4LightVehicle      ");
		heavy4HeavyVehicle.Log("heavy4HeavyVehicle      ");

		leader5CrossVehicle.Log("leader5CrossVehicle     ");

		leader7CrossVehicle .Log("leader7CrossVehicle     ");
		leader7Hench1Vehicle.Log("leader7Hench1Vehicle    ");
		leader7Hench2Vehicle.Log("leader7Hench2Vehicle    ");
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		rivalRoadblockEnabled.SetToHeatState(isRacing, heatLevel);
		rivalHeavyEnabled    .SetToHeatState(isRacing, heatLevel);
		rivalLeaderEnabled   .SetToHeatState(isRacing, heatLevel);

		roadblockCooldown     .SetToHeatState(isRacing, heatLevel);
		roadblockHeavyCooldown.SetToHeatState(isRacing, heatLevel);

		roadblockSpawnDistance.SetToHeatState(isRacing, heatLevel);
		roadblockEndsFormation.SetToHeatState(isRacing, heatLevel);

		maxRBJoinDistance      .SetToHeatState(isRacing, heatLevel);
		maxRBJoinElevationDelta.SetToHeatState(isRacing, heatLevel);
		maxRBJoinCount         .SetToHeatState(isRacing, heatLevel);

		regularRBJoinTimer.SetToHeatState(isRacing, heatLevel);
		backupRBJoinTimer .SetToHeatState(isRacing, heatLevel);

		reactToCooldownMode.SetToHeatState(isRacing, heatLevel);
		reactToSpikesHit   .SetToHeatState(isRacing, heatLevel);

		strategyCooldown.SetToHeatState(isRacing, heatLevel);

		heavy3SpeedLimit.SetToHeatState(isRacing, heatLevel);

		rammingSpeedLimit = heavy3SpeedLimit.current / 3.6f;

		heavy3TriggerCooldown.SetToHeatState(isRacing, heatLevel);
		heavy3AreBlockable   .SetToHeatState(isRacing, heatLevel);

		heavy3LightVehicle.SetToHeatState(isRacing, heatLevel);
		heavy3HeavyVehicle.SetToHeatState(isRacing, heatLevel);

		heavy4LightVehicle.SetToHeatState(isRacing, heatLevel);
		heavy4HeavyVehicle.SetToHeatState(isRacing, heatLevel);

		leader5CrossVehicle.SetToHeatState(isRacing, heatLevel);

		leader7CrossVehicle .SetToHeatState(isRacing, heatLevel);
		leader7Hench1Vehicle.SetToHeatState(isRacing, heatLevel);
		leader7Hench2Vehicle.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}
}