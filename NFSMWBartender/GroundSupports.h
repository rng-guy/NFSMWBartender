#pragma once

#include <string>

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
	HeatParameters::Pair<bool> rivalRoadblockEnableds(true);
	HeatParameters::Pair<bool> rivalHeavyEnableds    (true);
	HeatParameters::Pair<bool> rivalLeaderEnableds   (true);

	HeatParameters::Interval<float> roadblockCooldowns     (8.f,  12.f, {1.f}); // seconds
	HeatParameters::Pair    <float> roadblockHeavyCooldowns(15.f, {1.f});       // seconds

	HeatParameters::Interval<float> roadblockSpawnDistances(250.f, 250.f, {0.f}); // metres

	HeatParameters::Pair<bool> roadblockEndsFormations(true);

	HeatParameters::OptionalPair<float> roadblockJoinTimers; // seconds

	HeatParameters::Pair<float> maxRBJoinDistances      (500.f, {0.f}); // metres
	HeatParameters::Pair<float> maxRBJoinElevationDeltas(1.5f,  {0.f}); // metres
	HeatParameters::Pair<int>   maxRBJoinCounts         (1,     {0});   // cars

	HeatParameters::Pair<bool> reactToCooldownModes(true);
	HeatParameters::Pair<bool> reactToSpikesHits   (true);
	
	HeatParameters::Interval<float> strategyCooldowns(10.f, 10.f, {1.f}); // seconds

	HeatParameters::Pair<bool> heavy3TriggerCooldowns(true);
	HeatParameters::Pair<bool> heavy3AreBlockables   (true);

	HeatParameters::PointerPair<std::string> heavy3LightVehicles("copsuvl");
	HeatParameters::PointerPair<std::string> heavy3HeavyVehicles("copsuv");

	HeatParameters::PointerPair<std::string> heavy4LightVehicles("copsuvl");
	HeatParameters::PointerPair<std::string> heavy4HeavyVehicles("copsuv");

	HeatParameters::PointerPair<std::string> leader5CrossVehicles("copcross");
	HeatParameters::PointerPair<std::string> leader7CrossVehicles("copcross");

	HeatParameters::PointerPair<std::string> leader7Hench1Vehicles("copsporthench");
	HeatParameters::PointerPair<std::string> leader7Hench2Vehicles("copsporthench");





	// Auxiliary functions -----------------------------------------------------------------------------------------------------------------------------

	bool IsHeavyStrategyAvailable
	(
		const address pursuit,
		const address strategy
	) {
		const int  strategyID   = *reinterpret_cast<volatile int*>    (strategy);
		const bool hasRoadblock = *reinterpret_cast<volatile address*>(pursuit + 0x84);

		switch (strategyID)
		{
		case 3:
			return (not (hasRoadblock and heavy3AreBlockables.current));

		case 4:
			return (not hasRoadblock);
		}

		return false;
	}



	bool IsLeaderStrategyAvailable
	(
		const address pursuit,
		const address strategy
	) {
		const int crossFlag = *reinterpret_cast<volatile int*>(pursuit + 0x164);

		if (crossFlag == 0)
		{
			const int strategyID = *reinterpret_cast<volatile int*>(strategy);

			switch (strategyID)
			{
			case 5:
				[[fallthrough]];

			case 7:
				return true;
			}
		}

		return false;
	}



	template <address getNumStrategies, address getStrategy, auto IsStrategyAvailable>
	void MarshalStrategies
	(
		const address         pursuit,
		std::vector<address>& candidates
	) {
		const auto    GetSupportNode = reinterpret_cast<address (__thiscall*)(address)>(0x418EE0);
		const address supportNode    = GetSupportNode(pursuit - 0x48);

		if (not supportNode) return; // should never happen

		const auto GetNumStrategies = reinterpret_cast<size_t  (__thiscall*)(address)>        (getNumStrategies);
		const auto GetStrategy      = reinterpret_cast<address (__thiscall*)(address, size_t)>(getStrategy);

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; ++strategyID)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsStrategyAvailable(pursuit, strategy)) continue;

			const int chance = *reinterpret_cast<volatile int*>(strategy + 0x4);

			if (Globals::prng.DoTrial<int>(chance))
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
		static std::vector<address> candidates;

		// Marshal all currently eligible Strategies
		const bool isPlayerPursuit = Globals::IsPlayerPursuit(pursuit);

		if (isPlayerPursuit or rivalHeavyEnableds.current)
			MarshalStrategies<0x403600, 0x4035E0, IsHeavyStrategyAvailable>(pursuit, candidates);

		const size_t numHeavyStrategies = candidates.size();

		if (isPlayerPursuit or rivalLeaderEnableds.current)
			MarshalStrategies<0x403680, 0x403660, IsLeaderStrategyAvailable>(pursuit, candidates);

		// Attempt to select an eligible Strategy at random
		if (not candidates.empty())
		{
			const size_t index = Globals::prng.GenerateIndex(candidates.size());

			const address randomStrategy  = candidates[index];
			const bool    isHeavyStrategy = (index < numHeavyStrategies);

			SetStrategy(pursuit, randomStrategy, isHeavyStrategy);

			if constexpr (Globals::loggingEnabled)
			{
				const int strategyID = *reinterpret_cast<volatile int*>(randomStrategy);

				Globals::logger.Log(pursuit, "[SUP] Requesting", (isHeavyStrategy) ? "HeavyStrategy" : "LeaderStrategy", strategyID);
				Globals::logger.Log(pursuit, "[SUP] Candidate", static_cast<int>(index + 1), '/', static_cast<int>(candidates.size()));
			}

			candidates.clear();

			return true;
		}
		else if constexpr (Globals::loggingEnabled)
		{
			const bool canMakeRequest = (isPlayerPursuit or rivalHeavyEnableds.current or rivalLeaderEnableds.current);
			Globals::logger.Log(pursuit, "[SUP] Strategy request failed", (canMakeRequest) ? "(chance)" : "(blocked)");
		}

		return false;
	}



	bool __fastcall AcceptsRoadblockVehicles(const address pursuit)
	{
		const int numVehiclesJoined = *reinterpret_cast<volatile int*>(pursuit + 0x23C);
		if (numVehiclesJoined >= maxRBJoinCounts.current) return false;

		const float distanceToRoadblock = *reinterpret_cast<volatile float*>(pursuit + 0x7C);
		if (distanceToRoadblock > maxRBJoinDistances.current) return false;

		// Consult ChasersManager for cop capacity (if available)
		if (CopSpawnOverrides::featureEnabled)
		{
			if (not CopSpawnOverrides::ChasersManager::HasJoinCapacity(pursuit)) 
				return false;

			else if (CopSpawnOverrides::chasersAreIndependents.current) 
				return true;
		}

		// Fall back to counting cop capacity manually
		int numNonRoadblockVehicles = 0;

		if (Globals::copManager)
		{
			numNonRoadblockVehicles = *reinterpret_cast<volatile int*>(Globals::copManager + 0x94); // cops loaded

			const address finalPursuitListEntry   = *reinterpret_cast<volatile address*>(Globals::copManager + 0x128);
			address       currentPursuitListEntry = *reinterpret_cast<volatile address*>(finalPursuitListEntry);

			// Check each active pursuit for roadblock(s)
			while (currentPursuitListEntry != finalPursuitListEntry)
			{
				const address pursuit   = *reinterpret_cast<volatile address*>(currentPursuitListEntry + 0x8);
				const address roadblock = *reinterpret_cast<volatile address*>(pursuit + 0x84);

				if (roadblock)
				{
					const address firstVehiclePointer = *reinterpret_cast<volatile address*>(roadblock + 0xC);
					const address lastVehiclePointer  = *reinterpret_cast<volatile address*>(roadblock + 0x10);

					// Subtract roadblock-vehicle count
					if (lastVehiclePointer > firstVehiclePointer)
						numNonRoadblockVehicles -= (lastVehiclePointer - firstVehiclePointer) / sizeof(address);
				}

				currentPursuitListEntry = *reinterpret_cast<volatile address*>(currentPursuitListEntry);
			}
		}

		// Contains vanilla limit if ChasersManager is disabled
		return (numNonRoadblockVehicles < CopSpawnOverrides::activeChaserCounts.maxValues.current);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address crossSubEntrance = 0x41F504;
	constexpr address crossSubExit     = 0x41F50C;

	// Replaces Cross' vehicle
	__declspec(naked) void CrossSub()
	{
		__asm
		{
			mov ecx, dword ptr leader5CrossVehicles.current
			cmovne ecx, dword ptr leader7CrossVehicles.current // not LeaderStrategy 5

			mov dword ptr [esp + 0x24], ecx

			jmp dword ptr crossSubExit
		}
	}



	constexpr address onAttachedEntrance = 0x424036;
	constexpr address onAttachedExit     = 0x42403C;

	// Updates the Cross flag whenever a Cross replacement joins
	__declspec(naked) void OnAttached()
	{
		__asm
		{
			cmp byte ptr LeaderOverrides::featureEnabled, 0x1
			je conclusion // Cross flag managed by "Advanced" feature set

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

			mov dword ptr [esp + 0x24], eax
			mov dword ptr [esp + 0x28], edx

			mov esi, dword ptr [esp + 0x60]

			jmp dword ptr henchmenSubExit
		}
	}



	constexpr address spikeCounterEntrance = 0x43E654;
	constexpr address spikeCounterExit     = 0x43E663;

	// Increments the "spikes deployed" counter correctly
	__declspec(naked) void SpikeCounter()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x10]
			cmp dword ptr [ecx], 0x3 // prop ID
			jne conclusion           // prop not spike strip

			mov edx, dword ptr [esp + 0x4C4] // roadblock pursuit
			inc dword ptr [edx + 0x17C]      // spike strips deployed

			conclusion:
			jmp dword ptr spikeCounterExit
		}
	}



	constexpr address heavySelectorEntrance = 0x41F1BC;
	constexpr address heavySelectorExit     = 0x41F1C8;

	// Replaces HeavyStrategy vehicles
	__declspec(naked) void HeavySelector()
	{
		__asm
		{
			mov eax, dword ptr heavy3LightVehicles.current
			cmovl eax, dword ptr heavy3HeavyVehicles.current // is "heavy"

			mov edx, dword ptr heavy4LightVehicles.current
			cmovl edx, dword ptr heavy4HeavyVehicles.current // is "heavy"

			mov ecx, dword ptr [esi] // Strategy ID
			cmp dword ptr [ecx], 0x3
			cmovne eax, edx          // not HeavyStrategy 3

			jmp dword ptr heavySelectorExit
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
			call AcceptsRoadblockVehicles // ecx: pursuit
			test al, al
			je skip                       // may not join

			jmp dword ptr roadblockJoinCountExit

			skip:
			jmp dword ptr roadblockJoinCountSkip
		}
	}



	constexpr address roadblockJoinTimerEntrance = 0x42BF09;
	constexpr address roadblockJoinTimerExit     = 0x42BF14;

	// Checks the timer for joining from roadblocks
	__declspec(naked) void RoadblockJoinTimer()
	{
		__asm
		{
			cmp byte ptr roadblockJoinTimers.isEnableds.current, 0x1
			jne conclusion // time-based joining disabled

			fcom dword ptr roadblockJoinTimers.values.current
			fnstsw ax
			test ah, 0x41

			conclusion:
			fstp st(0)

			jmp dword ptr roadblockJoinTimerExit
		}
	}



	constexpr address cooldownModeReactionEntrance = 0x42BF16;
	constexpr address cooldownModeReactionExit     = 0x42BF2B;

	// Can suppress roadblock reactions to "COOLDOWN" mode
	__declspec(naked) void CooldownModeReaction()
	{
		__asm
		{
			cmp byte ptr reactToCooldownModes.current, 0x1
			jne conclusion // no "COOLDOWN"-mode reaction

			// Execute original code and resume
			mov eax, dword ptr [ebp + 0x28]  // roadblock pursuit
			cmp dword ptr [eax + 0x218], 0x2 // pursuit status

			conclusion:
			jmp dword ptr cooldownModeReactionExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Fixes incorrect spike-strip CTS count
		MemoryTools::MakeRangeJMP(SpikeCounter, spikeCounterEntrance, spikeCounterExit);

		// Also fixes the unintended biases in the Strategy-selection process
		MemoryTools::MakeRangeJMP(StrategySelection, strategySelectionEntrance, strategySelectionExit);

		// Also prevents excessive joining from roadblocks
		MemoryTools::MakeRangeJMP(RoadblockJoinCount, roadblockJoinCountEntrance, roadblockJoinCountExit);
	}



    bool Initialise(HeatParameters::Parser& parser)
    {
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupports");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Supports.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Supports:Rivals", rivalRoadblockEnableds, rivalHeavyEnableds, rivalLeaderEnableds);

		HeatParameters::Parse(parser, "Roadblocks:Cooldown",   roadblockCooldowns, roadblockHeavyCooldowns);
		HeatParameters::Parse(parser, "Roadblocks:Distance",   roadblockSpawnDistances);
		HeatParameters::Parse(parser, "Roadblocks:Formations", roadblockEndsFormations);
		HeatParameters::Parse(parser, "Roadblocks:Joining",    roadblockJoinTimers);
		HeatParameters::Parse(parser, "Roadblocks:Reactions",  reactToCooldownModes, reactToSpikesHits);
		HeatParameters::Parse(parser, "Joining:Definitions",   maxRBJoinDistances, maxRBJoinElevationDeltas, maxRBJoinCounts);

		HeatParameters::Parse(parser, "Strategies:Cooldown", strategyCooldowns);
		HeatParameters::Parse(parser, "Heavy3:Roadblocks",   heavy3TriggerCooldowns, heavy3AreBlockables);
		HeatParameters::Parse(parser, "Heavy3:Vehicles",     heavy3LightVehicles, heavy3HeavyVehicles);
		HeatParameters::Parse(parser, "Heavy4:Vehicles",     heavy4LightVehicles, heavy4HeavyVehicles);
		HeatParameters::Parse(parser, "Leader5:Vehicle",     leader5CrossVehicles);
		HeatParameters::Parse(parser, "Leader7:Vehicles",    leader7CrossVehicles, leader7Hench1Vehicles, leader7Hench2Vehicles);

		// Check whether vehicles are cars
		bool allValid = true;
		
		allValid &= HeatParameters::ValidateVehicles("Heavy 3, light", heavy3LightVehicles, Globals::IsVehicleCar);
		allValid &= HeatParameters::ValidateVehicles("Heavy 3, heavy", heavy3HeavyVehicles, Globals::IsVehicleCar);
		allValid &= HeatParameters::ValidateVehicles("Heavy 4, light", heavy4LightVehicles, Globals::IsVehicleCar);
		allValid &= HeatParameters::ValidateVehicles("Heavy 4, heavy", heavy4HeavyVehicles, Globals::IsVehicleCar);

		allValid &= HeatParameters::ValidateVehicles("Leader 5, Cross",   leader5CrossVehicles,  Globals::IsVehicleCar);
		allValid &= HeatParameters::ValidateVehicles("Leader 7, Cross",   leader7CrossVehicles,  Globals::IsVehicleCar);
		allValid &= HeatParameters::ValidateVehicles("Leader 7, hench 1", leader7Hench1Vehicles, Globals::IsVehicleCar);
		allValid &= HeatParameters::ValidateVehicles("Leader 7, hench 2", leader7Hench2Vehicles, Globals::IsVehicleCar);

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.Log<2>("All vehicles valid");
		}

		// Code modifications 
		MemoryTools::Write<float*>(&(maxRBJoinDistances.current),       {0x42BEBC});
		MemoryTools::Write<float*>(&(maxRBJoinElevationDeltas.current), {0x42BE3A});

		MemoryTools::MakeRangeNOP(0x42BEB6, 0x42BEBA); // roadblock-joining flag reset
		MemoryTools::MakeRangeNOP(0x42402A, 0x424036); // Cross flag = 1

		MemoryTools::MakeRangeJMP(CrossSub,             crossSubEntrance,             crossSubExit);
		MemoryTools::MakeRangeJMP(OnAttached,           onAttachedEntrance,           onAttachedExit);
		MemoryTools::MakeRangeJMP(OnDetached,           onDetachedEntrance,           onDetachedExit);
		MemoryTools::MakeRangeJMP(CrossSpawn,           crossSpawnEntrance,           crossSpawnExit);
		MemoryTools::MakeRangeJMP(HenchmenSub,          henchmenSubEntrance,          henchmenSubExit);
		MemoryTools::MakeRangeJMP(HeavySelector,        heavySelectorEntrance,        heavySelectorExit);
		MemoryTools::MakeRangeJMP(CrossPriority,        crossPriorityEntrance,        crossPriorityExit);
		MemoryTools::MakeRangeJMP(RivalRoadblock,       rivalRoadblockEntrance,       rivalRoadblockExit);
		MemoryTools::MakeRangeJMP(RequestCooldown,      requestCooldownEntrance,      requestCooldownExit);
		MemoryTools::MakeRangeJMP(RoadblockCooldown,    roadblockCooldownEntrance,    roadblockCooldownExit);
		MemoryTools::MakeRangeJMP(RoadblockDistance,    roadblockDistanceEntrance,    roadblockDistanceExit);
		MemoryTools::MakeRangeJMP(SpikesHitReaction,    spikesHitReactionEntrance,    spikesHitReactionExit);
		MemoryTools::MakeRangeJMP(RoadblockFormation,   roadblockFormationEntrance,   roadblockFormationExit);
		MemoryTools::MakeRangeJMP(RoadblockJoinTimer,   roadblockJoinTimerEntrance,   roadblockJoinTimerExit);
		MemoryTools::MakeRangeJMP(CooldownModeReaction, cooldownModeReactionEntrance, cooldownModeReactionExit);
        
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

		if (roadblockJoinTimers.isEnableds.current or reactToCooldownModes.current)
		{
			maxRBJoinDistances      .Log("maxRBJoinDistance       ");
			maxRBJoinElevationDeltas.Log("maxRBJoinElevationDeltas");
			maxRBJoinCounts         .Log("maxRBJoinCount          ");
		}

		roadblockJoinTimers.Log("roadblockJoinTimer      ");

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

		roadblockJoinTimers.SetToHeat(isRacing, heatLevel);

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