#pragma once

#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace GroundSupport
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool> rivalRoadblocksEnableds(true);
	HeatParameters::Pair<bool> rivalHeaviesEnableds   (true);
	HeatParameters::Pair<bool> rivalLeadersEnableds   (true);

	HeatParameters::Pair<float> minRoadblockCooldowns  (8.f);  // seconds
	HeatParameters::Pair<float> maxRoadblockCooldowns  (12.f);
	HeatParameters::Pair<float> roadblockHeavyCooldowns(15.f);

	HeatParameters::Pair<float> strategyCooldowns(10.f); // seconds

	HeatParameters::Pair<bool>  roadblockJoinEnableds    (false);
	HeatParameters::Pair<float> roadblockJoinTimers      (20.f);  // seconds
	HeatParameters::Pair<float> maxRoadblockJoinDistances(100.f); // metres

	HeatParameters::Pair<bool> reactToCooldownModes(true);
	HeatParameters::Pair<bool> reactToSpikesHits   (true);
	
	HeatParameters::Pair<bool> heavy3TriggerCooldowns(true);
	HeatParameters::Pair<bool> heavy3AreBlockables   (true);

	HeatParameters::Pair<std::string> heavy3LightVehicles   ("copsuvl");
	HeatParameters::Pair<std::string> heavy3HeavyVehicles   ("copsuv");
	HeatParameters::Pair<std::string> heavy4LightVehicles   ("copsuvl");
	HeatParameters::Pair<std::string> heavy4HeavyVehicles   ("copsuv");
	HeatParameters::Pair<std::string> leaderCrossVehicles   ("copcross");
	HeatParameters::Pair<std::string> leaderHenchmenVehicles("copsporthench");

	// Conversions
	float roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current; // seconds





	// Auxiliary functions -----------------------------------------------------------------------------------------------------------------------------

	bool IsHeavyStrategyAvailable
	(
		const address pursuit,
		const address strategy
	) {
		const int strategyType = *((int*)strategy);

		switch (strategyType)
		{
		case 3:
			if (not heavy3AreBlockables.current) return true;
			[[fallthrough]];

		case 4:
			return (not *((address*)(pursuit + 0x84))); // active roadblock
		}

		return false;
	}



	bool IsLeaderStrategyAvailable
	(
		const address pursuit,
		const address strategy
	) {
		if (*((int*)(pursuit + 0x164)) != 0) return false; // leader spawn-flag

		const int strategyType = *((int*)strategy);

		switch (strategyType)
		{
		case 5:
			[[fallthrough]];

		case 7:
			return true;
		}

		return false;
	}



	template <address getNumStrategies, address getStrategy, auto IsStrategyAvailable>
	void MarshalStrategies
	(
		const address         pursuit,
		std::vector<address>& candidateStrategies
	) {
		static const auto GetSupportNode = (address (__thiscall*)(address))0x418EE0;
		const address     supportNode    = GetSupportNode(pursuit - 0x48);

		if (not supportNode) return;

		static const auto GetNumStrategies = (size_t  (__thiscall*)(address))        getNumStrategies;
		static const auto GetStrategy      = (address (__thiscall*)(address, size_t))getStrategy;

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; ++strategyID)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsStrategyAvailable(pursuit, strategy)) continue;

			const int chance = *((int*)(strategy + 0x4));

			if (Globals::prng.GenerateNumber<int>(1, 100) <= chance)
				candidateStrategies.push_back(strategy);
		}
	}



	void SetStrategy
	(
		const address pursuit,
		const address strategy,
		const bool    isHeavyStrategy
	) {
		*((float*)(pursuit + 0x208)) = *((float*)(strategy + 0x8)); // duration
		*((int*)(pursuit + 0x20C))   = 1;                           // request flag

		if (isHeavyStrategy)
		{
			*((address*)(pursuit + 0x194)) = strategy; // HeavyStrategy

			if (heavy3TriggerCooldowns.current or (*((int*)strategy) != 3))
				*((float*)(pursuit + 0xC8)) = roadblockHeavyCooldowns.current; // roadblock CD
		}
		else *((address*)(pursuit + 0x198)) = strategy; // LeaderStrategy
	}



	bool __fastcall SetRandomStrategy(const address pursuit) 
	{
		static std::vector<address> candidateStrategies;

		const bool rivalTestRequired = (not (rivalHeaviesEnableds.current and rivalLeadersEnableds.current));
		const bool isPlayerPursuit   = ((not rivalTestRequired) or Globals::IsPlayerPursuit(pursuit));

		if (isPlayerPursuit or rivalHeaviesEnableds.current)
			MarshalStrategies<0x403600, 0x4035E0, IsHeavyStrategyAvailable>(pursuit, candidateStrategies);

		const size_t numHeavyStrategies = candidateStrategies.size();

		if (isPlayerPursuit or rivalLeadersEnableds.current)
			MarshalStrategies<0x403680, 0x403660, IsLeaderStrategyAvailable>(pursuit, candidateStrategies);

		if (not candidateStrategies.empty())
		{
			const size_t  index           = Globals::prng.GenerateIndex(candidateStrategies.size());
			const address randomStrategy  = candidateStrategies[index];
			const bool    isHeavyStrategy = (index < numHeavyStrategies);

			SetStrategy(pursuit, randomStrategy, isHeavyStrategy);

			if constexpr (Globals::loggingEnabled)
			{
				Globals::logger.Log(pursuit, "[SUP] Requesting", (isHeavyStrategy) ? "HeavyStrategy" : "LeaderStrategy", *((int*)randomStrategy));
				Globals::logger.Log(pursuit, "[SUP] Candidate", (int)(index + 1), "out of", (int)candidateStrategies.size());
			}

			candidateStrategies.clear();

			return true;
		}
		else if constexpr (Globals::loggingEnabled)
		{
			const char* const reason = (isPlayerPursuit or rivalHeaviesEnableds.current or rivalLeadersEnableds.current) ? "(chance)" : "(blocked)";
			Globals::logger.Log(pursuit, "[SUP] Strategy request failed", reason);
		}

		return false;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address cooldownModeReactionEntrance = 0x42BF16;
	constexpr address cooldownModeReactionExit     = 0x42BF2B;

	__declspec(naked) void CooldownModeReaction()
	{
		__asm
		{
			cmp byte ptr reactToCooldownModes.current, 0x1
			jne conclusion // no "COOLDOWN"-mode reaction

			// Execute original code and resume
			mov edx, dword ptr [ebp]
			mov ecx, ebp
			call dword ptr [edx + 0x1C]

			mov edx, dword ptr [eax]
			mov ecx, eax
			call dword ptr [edx + 0x114]

			cmp eax, 0x2

			conclusion:
			jmp dword ptr cooldownModeReactionExit
		}
	}



	constexpr address spikesHitReactionEntrance = 0x63BB92;
	constexpr address spikesHitReactionExit     = 0x63BB98;

	__declspec(naked) void SpikesHitReaction()
	{
		__asm
		{
			// Execute original code first
			test eax, eax
			mov dword ptr [esp + 0x10], eax
			je conclusion // no pursuit or roadblock

			cmp byte ptr reactToSpikesHits.current, 0x0

			conclusion:
			jmp dword ptr spikesHitReactionExit
		}
	}



	constexpr address strategySelectionEntrance = 0x41978D;
	constexpr address strategySelectionExit     = 0x41984E;

	__declspec(naked) void StrategySelection()
	{
		__asm
		{
			mov ecx, esi
			call SetRandomStrategy // ecx: pursuit
			test al, al
			je conclusion          // no Strategy set

			mov edi, -0x1

			conclusion:
			jmp dword ptr strategySelectionExit
		}
	}



	constexpr address roadblockCooldownEntrance = 0x419535;
	constexpr address roadblockCooldownExit     = 0x41953A;

	__declspec(naked) void RoadblockCooldown()
	{
		__asm
		{
			push dword ptr roadblockCooldownRange

			jmp dword ptr roadblockCooldownExit
		}
	}



	constexpr address requestCooldownEntrance = 0x4196DA;
	constexpr address requestCooldownExit     = 0x4196E4;

	__declspec(naked) void RequestCooldown()
	{
		__asm
		{
			mov eax, dword ptr strategyCooldowns.current
			mov dword ptr [esi + 0x210], eax

			jmp dword ptr requestCooldownExit
		}
	}



	constexpr address crossPriorityEntrance = 0x419724;
	constexpr address crossPriorityExit     = 0x41972A;

	__declspec(naked) void CrossPriority()
	{
		static constexpr address crossPrioritySkip = 0x419780;

		__asm
		{
			jne skip // priority flag set

			cmp byte ptr rivalLeadersEnableds.current, 0x1
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



	constexpr address roadblockJoinEntrance = 0x42BF09;
	constexpr address roadblockJoinExit     = 0x42BF14;

	__declspec(naked) void RoadblockJoin()
	{
		__asm
		{
			cmp byte ptr roadblockJoinEnableds.current, 0x1
			jne skip // vehicles may not join

			fcomp dword ptr roadblockJoinTimers.current
			fnstsw ax
			test ah, 0x41
			jne conclusion // timer has yet to expire

			fld dword ptr maxRoadblockJoinDistances.current
			fcomp dword ptr [esp + 0x14]
			fnstsw ax
			test ah, 0x1

			jmp conclusion // vehicle may join if close enough

			skip:
			fstp st(0)

			conclusion:
			jmp dword ptr roadblockJoinExit
		}
	}



	constexpr address rivalRoadblockEntrance = 0x419563;
	constexpr address rivalRoadblockExit     = 0x419568;

	__declspec(naked) void RivalRoadblock()
	{
		static constexpr address rivalRoadblockSkip = 0x4195CD;

		__asm
		{
			cmp byte ptr rivalRoadblocksEnableds.current, 0x1
			je conclusion // no rival discrimination

			call dword ptr [edx + 0x8C]
			test eax, eax
			jne restore // is player pursuit

			jmp dword ptr rivalRoadblockSkip // was rival pursuit

			restore:
			mov ecx, esi
			mov edx, dword ptr [esi]

			conclusion:
			// Execute original code and resume
			call dword ptr [edx + 0x28]
			cmp al, 0x1

			jmp dword ptr rivalRoadblockExit
		}
	}



	constexpr address onAttachedEntrance = 0x423F92;
	constexpr address onAttachedExit     = 0x423F97;

	__declspec(naked) void OnAttached()
	{
		__asm
		{
			push dword ptr leaderCrossVehicles.current

			jmp dword ptr onAttachedExit
		}
	}



	constexpr address onDetachedEntrance = 0x42B5F1;
	constexpr address onDetachedExit     = 0x42B5F6;

	__declspec(naked) void OnDetached()
	{
		__asm
		{
			push dword ptr leaderCrossVehicles.current

			jmp dword ptr onDetachedExit
		}
	}



	constexpr address leaderSubEntrance = 0x41F504;
	constexpr address leaderSubExit     = 0x41F50C;

	__declspec(naked) void LeaderSub()
	{
		__asm
		{
			mov ecx, dword ptr leaderCrossVehicles.current
			mov dword ptr [esp + 0x24], ecx

			jmp dword ptr leaderSubExit
		}
	}



	constexpr address henchmenSubEntrance = 0x41F485;
	constexpr address henchmenSubExit     = 0x41F48A;

	__declspec(naked) void HenchmenSub()
	{
		__asm
		{
			mov eax, dword ptr leaderHenchmenVehicles.current

			jmp dword ptr henchmenSubExit
		}
	}



	constexpr address rhinoSelectorEntrance = 0x41F1BC;
	constexpr address rhinoSelectorExit     = 0x41F1C8;

	__declspec(naked) void RhinoSelector()
	{
		__asm
		{
			mov eax, dword ptr [esi]
			jl heavy // is "heavy" Rhino variant

			cmp byte ptr [eax], 0x3
			mov eax, dword ptr heavy3LightVehicles.current
			je conclusion  // is HeavyStrategy 3
			mov eax, dword ptr heavy4LightVehicles.current
			jmp conclusion // is HeavyStrategy 4

			heavy:
			cmp byte ptr [eax], 0x3
			mov eax, dword ptr heavy3HeavyVehicles.current
			je conclusion
			mov eax, dword ptr heavy4HeavyVehicles.current

			conclusion:
			jmp dword ptr rhinoSelectorExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    bool Initialise(HeatParameters::Parser& parser)
    {
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Supports.ini")) return false;

		// Roadblock timers
		HeatParameters::Parse<float, float, float>
		(
			parser,
			"Roadblocks:Cooldown",
			{minRoadblockCooldowns,   1.f},
			{maxRoadblockCooldowns,   1.f},
			{roadblockHeavyCooldowns, 1.f}
		);

		HeatParameters::CheckIntervals<float>(minRoadblockCooldowns, maxRoadblockCooldowns);

		// Roadblock join parameters
		HeatParameters::ParseOptional<float, float>
		(
			parser,
			"Roadblocks:Joining",
			roadblockJoinEnableds,
			{roadblockJoinTimers,       1.f},
			{maxRoadblockJoinDistances, 0.f}
		);

		// Other Heat parameters
		HeatParameters::Parse<bool, bool, bool>
		(
			parser, 
			"Supports:Rivals",      
			{rivalRoadblocksEnableds}, 
			{rivalHeaviesEnableds}, 
			{rivalLeadersEnableds}
		);

		HeatParameters::Parse<bool, bool>(parser, "Roadblocks:Reactions", {reactToCooldownModes},    {reactToSpikesHits});
		HeatParameters::Parse<bool, bool>(parser, "Heavy3:Roadblocks",    {heavy3TriggerCooldowns},  {heavy3AreBlockables});
		HeatParameters::Parse<float>     (parser, "Strategies:Cooldown",  {strategyCooldowns, 1.f});

		HeatParameters::Parse<std::string, std::string>(parser, "Heavy3:Vehicles", {heavy3LightVehicles}, {heavy3HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Heavy4:Vehicles", {heavy4LightVehicles}, {heavy4HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Leader:Vehicles", {leaderCrossVehicles}, {leaderHenchmenVehicles});

		// Code caves
		MemoryTools::Write<float*>(&(minRoadblockCooldowns.current), {0x419548});

		MemoryTools::DigCodeCave(CooldownModeReaction, cooldownModeReactionEntrance, cooldownModeReactionExit);
		MemoryTools::DigCodeCave(SpikesHitReaction,    spikesHitReactionEntrance,    spikesHitReactionExit);
		MemoryTools::DigCodeCave(StrategySelection,    strategySelectionEntrance,    strategySelectionExit);
		MemoryTools::DigCodeCave(RoadblockCooldown,    roadblockCooldownEntrance,    roadblockCooldownExit);
		MemoryTools::DigCodeCave(RivalRoadblock,       rivalRoadblockEntrance,       rivalRoadblockExit);
        MemoryTools::DigCodeCave(RequestCooldown,      requestCooldownEntrance,      requestCooldownExit);
		MemoryTools::DigCodeCave(RoadblockJoin,        roadblockJoinEntrance,        roadblockJoinExit);
		MemoryTools::DigCodeCave(CrossPriority,        crossPriorityEntrance,        crossPriorityExit);
		MemoryTools::DigCodeCave(OnAttached,           onAttachedEntrance,           onAttachedExit);
		MemoryTools::DigCodeCave(OnDetached,           onDetachedEntrance,           onDetachedExit);
        MemoryTools::DigCodeCave(LeaderSub,            leaderSubEntrance,            leaderSubExit);
		MemoryTools::DigCodeCave(HenchmenSub,          henchmenSubEntrance,          henchmenSubExit);
        MemoryTools::DigCodeCave(RhinoSelector,        rhinoSelectorEntrance,        rhinoSelectorExit);

		// Status flag
		featureEnabled = true;

		return true;
    }



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupport");

		// With logging disabled, the compiler optimises the boolean and the string literals away
		bool allValid = true;

		allValid &= heavy3LightVehicles   .Validate("HeavyStrategy 3, light",   Globals::Class::CAR);
		allValid &= heavy3HeavyVehicles   .Validate("HeavyStrategy 3, heavy",   Globals::Class::CAR);
		allValid &= heavy4LightVehicles   .Validate("HeavyStrategy 4, light",   Globals::Class::CAR);
		allValid &= heavy4HeavyVehicles   .Validate("HeavyStrategy 4, heavy",   Globals::Class::CAR);
		allValid &= leaderCrossVehicles   .Validate("LeaderStrategy, Cross",    Globals::Class::CAR);
		allValid &= leaderHenchmenVehicles.Validate("LeaderStrategy, henchmen", Globals::Class::CAR);

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.LogLongIndent("All vehicles valid");
		}
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [SUP] GroundSupport");

		Globals::logger.LogLongIndent("rivalRoadblocksEnabled  ", rivalRoadblocksEnableds.current);
		Globals::logger.LogLongIndent("rivalHeaviesEnabled     ", rivalHeaviesEnableds.current);
		Globals::logger.LogLongIndent("rivalLeadersEnabled     ", rivalLeadersEnableds.current);

		Globals::logger.LogLongIndent("roadblockCooldown       ", minRoadblockCooldowns.current, "to", maxRoadblockCooldowns.current);
		Globals::logger.LogLongIndent("roadblockHeavyCooldown  ", roadblockHeavyCooldowns.current);

		Globals::logger.LogLongIndent("strategyCooldown        ", strategyCooldowns.current);

		if (roadblockJoinEnableds.current)
		{
			Globals::logger.LogLongIndent("roadblockJoinTimer      ", roadblockJoinTimers.current);
			Globals::logger.LogLongIndent("maxRoadblockJoinDistance", maxRoadblockJoinDistances.current);
		}

		Globals::logger.LogLongIndent("reactToCooldownMode     ", reactToCooldownModes.current);
		Globals::logger.LogLongIndent("reactToSpikesHit        ", reactToSpikesHits.current);

		Globals::logger.LogLongIndent("heavy3TriggerCooldown   ", heavy3TriggerCooldowns.current);
		Globals::logger.LogLongIndent("heavy3AreBlockable      ", heavy3AreBlockables.current);

		Globals::logger.LogLongIndent("heavy3LightVehicle      ", heavy3LightVehicles.current);
		Globals::logger.LogLongIndent("heavy3HeavyVehicle      ", heavy3HeavyVehicles.current);
		Globals::logger.LogLongIndent("heavy4LightVehicle      ", heavy4LightVehicles.current);
		Globals::logger.LogLongIndent("heavy4HeavyVehicle      ", heavy4HeavyVehicles.current);
		Globals::logger.LogLongIndent("leaderCrossVehicle      ", leaderCrossVehicles.current);
		Globals::logger.LogLongIndent("leaderHenchmenVehicle   ", leaderHenchmenVehicles.current);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		rivalRoadblocksEnableds.SetToHeat(isRacing, heatLevel);
		rivalHeaviesEnableds   .SetToHeat(isRacing, heatLevel);
		rivalLeadersEnableds   .SetToHeat(isRacing, heatLevel);

		minRoadblockCooldowns  .SetToHeat(isRacing, heatLevel);
		maxRoadblockCooldowns  .SetToHeat(isRacing, heatLevel);
		roadblockHeavyCooldowns.SetToHeat(isRacing, heatLevel);

		strategyCooldowns.SetToHeat(isRacing, heatLevel);

		roadblockJoinEnableds    .SetToHeat(isRacing, heatLevel);
		roadblockJoinTimers      .SetToHeat(isRacing, heatLevel);
		maxRoadblockJoinDistances.SetToHeat(isRacing, heatLevel);

		reactToCooldownModes.SetToHeat(isRacing, heatLevel);
		reactToSpikesHits   .SetToHeat(isRacing, heatLevel);

		heavy3TriggerCooldowns.SetToHeat(isRacing, heatLevel);
		heavy3AreBlockables   .SetToHeat(isRacing, heatLevel);

		heavy3LightVehicles   .SetToHeat(isRacing, heatLevel);
		heavy3HeavyVehicles   .SetToHeat(isRacing, heatLevel);
		heavy4LightVehicles   .SetToHeat(isRacing, heatLevel);
		heavy4HeavyVehicles   .SetToHeat(isRacing, heatLevel);
		leaderCrossVehicles   .SetToHeat(isRacing, heatLevel);
		leaderHenchmenVehicles.SetToHeat(isRacing, heatLevel);

		roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current;

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
    }
}