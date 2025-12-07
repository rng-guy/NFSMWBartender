#pragma once

#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace GroundSupports
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool> rivalRoadblockEnableds(true);
	HeatParameters::Pair<bool> rivalHeavyEnableds    (true);
	HeatParameters::Pair<bool> rivalLeaderEnableds   (true);

	HeatParameters::Interval<float> roadblockCooldowns     (8.f, 12.f); // seconds
	HeatParameters::Pair    <float> roadblockHeavyCooldowns(15.f);

	HeatParameters::Pair<float> strategyCooldowns(10.f); // seconds

	HeatParameters::OptionalPair<float> roadblockJoinTimers;

	HeatParameters::Pair<float> maxRBJoinDistances      (500.f); // metres
	HeatParameters::Pair<float> maxRBJoinElevationDeltas(1.5f);
	HeatParameters::Pair<int>   maxRBJoinCounts         (1);

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





	// Auxiliary functions -----------------------------------------------------------------------------------------------------------------------------

	bool IsHeavyStrategyAvailable
	(
		const address pursuit,
		const address strategy
	) {
		const int     strategyID      = *reinterpret_cast<int*>    (strategy);
		const address activeRoadblock = *reinterpret_cast<address*>(pursuit + 0x84);

		switch (strategyID)
		{
		case 3:
			return (not (activeRoadblock and heavy3AreBlockables.current));

		case 4:
			return (not activeRoadblock);
		}

		return false;
	}



	bool IsLeaderStrategyAvailable
	(
		const address pursuit,
		const address strategy
	) {
		const int crossFlag = *reinterpret_cast<int*>(pursuit + 0x164);

		if (crossFlag == 0)
		{
			const int strategyID = *reinterpret_cast<int*>(strategy);

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
		std::vector<address>& candidateStrategies
	) {
		static const auto GetSupportNode = reinterpret_cast<address (__thiscall*)(address)>(0x418EE0);
		const address     supportNode    = GetSupportNode(pursuit - 0x48);

		if (not supportNode) return; // should never happen

		static const auto GetNumStrategies = reinterpret_cast<size_t  (__thiscall*)(address)>        (getNumStrategies);
		static const auto GetStrategy      = reinterpret_cast<address (__thiscall*)(address, size_t)>(getStrategy);

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; ++strategyID)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsStrategyAvailable(pursuit, strategy)) continue;

			const int chance = *reinterpret_cast<int*>(strategy + 0x4);

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
		const float duration = *reinterpret_cast<float*>(strategy + 0x8);

		*reinterpret_cast<float*>(pursuit + 0x208) = duration; // strategy duration
		*reinterpret_cast<int*>  (pursuit + 0x20C) = 1;        // request flag

		if (isHeavyStrategy)
		{
			const int strategyID = *reinterpret_cast<int*>(strategy);

			if ((strategyID != 3) or heavy3TriggerCooldowns.current)
				*reinterpret_cast<float*>(pursuit + 0xC8) = roadblockHeavyCooldowns.current; // roadblock CD

			*reinterpret_cast<address*>(pursuit + 0x194) = strategy; // HeavyStrategy
		}
		else *reinterpret_cast<address*>(pursuit + 0x198) = strategy; // LeaderStrategy
	}



	bool __fastcall SetRandomStrategy(const address pursuit) 
	{
		static std::vector<address> candidateStrategies;

		const bool isPlayerPursuit = Globals::IsPlayerPursuit(pursuit);

		if (isPlayerPursuit or rivalHeavyEnableds.current)
			MarshalStrategies<0x403600, 0x4035E0, IsHeavyStrategyAvailable>(pursuit, candidateStrategies);

		const size_t numHeavyStrategies = candidateStrategies.size();

		if (isPlayerPursuit or rivalLeaderEnableds.current)
			MarshalStrategies<0x403680, 0x403660, IsLeaderStrategyAvailable>(pursuit, candidateStrategies);

		if (not candidateStrategies.empty())
		{
			const size_t  index           = Globals::prng.GenerateIndex(candidateStrategies.size());
			const address randomStrategy  = candidateStrategies[index];
			const bool    isHeavyStrategy = (index < numHeavyStrategies);

			SetStrategy(pursuit, randomStrategy, isHeavyStrategy);

			if constexpr (Globals::loggingEnabled)
			{
				const int strategyID = *reinterpret_cast<int*>(randomStrategy);

				Globals::logger.Log(pursuit, "[SUP] Requesting", (isHeavyStrategy) ? "HeavyStrategy" : "LeaderStrategy", strategyID);
				Globals::logger.Log(pursuit, "[SUP] Candidate", static_cast<int>(index + 1), '/', static_cast<int>(candidateStrategies.size()));
			}

			candidateStrategies.clear();

			return true;
		}
		else if constexpr (Globals::loggingEnabled)
		{
			const bool canMakeRequest = (isPlayerPursuit or rivalHeavyEnableds.current or rivalLeaderEnableds.current);
			Globals::logger.Log(pursuit, "[SUP] Strategy request failed", (canMakeRequest) ? "(chance)" : "(blocked)");
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



	constexpr address roadblockJoinTimerEntrance = 0x42BF09;
	constexpr address roadblockJoinTimerExit     = 0x42BF14;

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



	constexpr address roadblockJoinCountEntrance = 0x4443A6;
	constexpr address roadblockJoinCountExit     = 0x4443AE;

	__declspec(naked) void RoadblockJoinCount()
	{
		static constexpr address roadblockJoinCountSkip = 0x444400;

		__asm
		{
			mov eax, dword ptr [esi + 0x27C]
			cmp eax, dword ptr maxRBJoinCounts.current
			jge skip // no more vehicles can join

			jmp dword ptr roadblockJoinCountExit

			skip:
			jmp dword ptr roadblockJoinCountSkip
		}
	}



	constexpr address spikesHitReactionEntrance = 0x63BB9E;
	constexpr address spikesHitReactionExit     = 0x63BBA6;

	__declspec(naked) void SpikesHitReaction()
	{
		static constexpr float maxJoinRange = 80.f; // metres

		__asm
		{
			// Execute original code first
			call dword ptr [edx + 0xA0]
			test eax, eax
			je conclusion // no pursuit

			cmp byte ptr reactToSpikesHits.current, 0x0
			je conclusion // reaction disabled

			mov edx, eax

			fld dword ptr [edx + 0x7C]
			fcomp dword ptr maxJoinRange
			fnstsw ax
			test ah, 0x41

			mov eax, edx

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
			xor edi, edi

			mov ecx, esi
			call SetRandomStrategy // ecx: pursuit
			test al, al
			je conclusion          // no Strategy set

			dec edi

			conclusion:
			jmp dword ptr strategySelectionExit
		}
	}



	constexpr address roadblockCooldownEntrance = 0x419535;
	constexpr address roadblockCooldownExit     = 0x41954C;

	__declspec(naked) void RoadblockCooldown()
	{
		__asm
		{
			mov ecx, offset roadblockCooldowns
			call HeatParameters::Interval<float>::GetRandomValue

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



	constexpr address rivalRoadblockEntrance = 0x419563;
	constexpr address rivalRoadblockExit     = 0x419568;

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
			call dword ptr [edx + 0x28]
			cmp al, 0x1

			jmp dword ptr rivalRoadblockExit

			skip:
			jmp dword ptr rivalRoadblockSkip
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



	constexpr address rhinoSelectorEntrance = 0x41F1BC;
	constexpr address rhinoSelectorExit     = 0x41F1C8;

	__declspec(naked) void RhinoSelector()
	{
		__asm
		{
			mov eax, dword ptr [esi]
			jl heavy // is "heavy" Rhino variant

			cmp dword ptr [eax], 0x3
			mov eax, dword ptr heavy3LightVehicles.current
			cmovne eax, dword ptr heavy4LightVehicles.current // not HeavyStrategy 3
			jmp conclusion                                    // was "light" variant

			heavy:
			cmp dword ptr [eax], 0x3
			mov eax, dword ptr heavy3HeavyVehicles.current
			cmovne eax, dword ptr heavy4HeavyVehicles.current // not HeavyStrategy 3

			conclusion:
			jmp dword ptr rhinoSelectorExit
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





    // State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		static bool fixesApplied = false;

		if (not fixesApplied)
		{
			// Also fixes the unintended biases in the Strategy-selection process
			MemoryTools::MakeRangeJMP(StrategySelection, strategySelectionEntrance, strategySelectionExit);

			fixesApplied = true;
		}
	}



    bool Initialise(HeatParameters::Parser& parser)
    {
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Supports.ini")) return false;

		// Roadblock cooldowns
		HeatParameters::Parse<float, float>
		(
			parser,
			"Roadblocks:Cooldown",
			{roadblockCooldowns,      1.f},
			{roadblockHeavyCooldowns, 1.f}
		);

		// Roadblock joining
		HeatParameters::Parse<float, float, int>
		(
			parser,
			"Joining:Definitions",
			{maxRBJoinDistances,       0.f},
			{maxRBJoinElevationDeltas, 0.f},
			{maxRBJoinCounts,          0}
		);

		HeatParameters::ParseOptional<float>(parser, "Roadblocks:Joining", {roadblockJoinTimers, .001f});

		// Other Heat parameters
		HeatParameters::Parse<bool, bool, bool>(parser, "Supports:Rivals", {rivalRoadblockEnableds}, {rivalHeavyEnableds}, {rivalLeaderEnableds});

		HeatParameters::Parse<bool, bool>(parser, "Roadblocks:Reactions", {reactToCooldownModes},    {reactToSpikesHits});
		HeatParameters::Parse<bool, bool>(parser, "Heavy3:Roadblocks",    {heavy3TriggerCooldowns},  {heavy3AreBlockables});
		HeatParameters::Parse<float>     (parser, "Strategies:Cooldown",  {strategyCooldowns, 1.f});

		HeatParameters::Parse<std::string, std::string>(parser, "Heavy3:Vehicles", {heavy3LightVehicles}, {heavy3HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Heavy4:Vehicles", {heavy4LightVehicles}, {heavy4HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Leader:Vehicles", {leaderCrossVehicles}, {leaderHenchmenVehicles});

		// Code modifications 
		MemoryTools::MakeRangeNOP(0x42BEB6, 0x42BEBA); // roadblock-joining flag reset

		MemoryTools::Write<float*>(&(maxRBJoinDistances.current),       {0x42BEBC});
		MemoryTools::Write<float*>(&(maxRBJoinElevationDeltas.current), {0x42BE3A});

		MemoryTools::MakeRangeJMP(CooldownModeReaction, cooldownModeReactionEntrance, cooldownModeReactionExit);
		MemoryTools::MakeRangeJMP(RoadblockJoinTimer,   roadblockJoinTimerEntrance,   roadblockJoinTimerExit);
		MemoryTools::MakeRangeJMP(RoadblockJoinCount,   roadblockJoinCountEntrance,   roadblockJoinCountExit);
		MemoryTools::MakeRangeJMP(SpikesHitReaction,    spikesHitReactionEntrance,    spikesHitReactionExit);
		MemoryTools::MakeRangeJMP(RoadblockCooldown,    roadblockCooldownEntrance,    roadblockCooldownExit);
		MemoryTools::MakeRangeJMP(RequestCooldown,      requestCooldownEntrance,      requestCooldownExit);
		MemoryTools::MakeRangeJMP(RivalRoadblock,       rivalRoadblockEntrance,       rivalRoadblockExit);
		MemoryTools::MakeRangeJMP(CrossPriority,        crossPriorityEntrance,        crossPriorityExit);
		MemoryTools::MakeRangeJMP(RhinoSelector,        rhinoSelectorEntrance,        rhinoSelectorExit);
		MemoryTools::MakeRangeJMP(HenchmenSub,          henchmenSubEntrance,          henchmenSubExit);
		MemoryTools::MakeRangeJMP(OnAttached,           onAttachedEntrance,           onAttachedExit);
		MemoryTools::MakeRangeJMP(OnDetached,           onDetachedEntrance,           onDetachedExit);
        MemoryTools::MakeRangeJMP(LeaderSub,            leaderSubEntrance,            leaderSubExit);

		ApplyFixes(); // also contains Strategy-selection feature
        
		// Status flag
		featureEnabled = true;

		return true;
    }



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupports");

		// With logging disabled, the compiler optimises the boolean and the string literals away
		bool allValid = true;

		allValid &= HeatParameters::ValidateVehicles("HeavyStrategy 3, light",   heavy3LightVehicles,    Globals::Class::CAR);
		allValid &= HeatParameters::ValidateVehicles("HeavyStrategy 3, heavy",   heavy3HeavyVehicles,    Globals::Class::CAR);
		allValid &= HeatParameters::ValidateVehicles("HeavyStrategy 4, light",   heavy4LightVehicles,    Globals::Class::CAR);
		allValid &= HeatParameters::ValidateVehicles("HeavyStrategy 4, heavy",   heavy4HeavyVehicles,    Globals::Class::CAR);
		allValid &= HeatParameters::ValidateVehicles("LeaderStrategy, Cross",    leaderCrossVehicles,    Globals::Class::CAR);
		allValid &= HeatParameters::ValidateVehicles("LeaderStrategy, henchmen", leaderHenchmenVehicles, Globals::Class::CAR);

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.LogLongIndent("All vehicles valid");
		}
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [SUP] GroundSupport");

		rivalRoadblockEnableds.Log("rivalRoadblockEnabled   ");
		rivalHeavyEnableds    .Log("rivalHeavyEnabled       ");
		rivalLeaderEnableds   .Log("rivalLeaderEnabled      ");

		roadblockCooldowns     .Log("roadblockCooldown       ");
		roadblockHeavyCooldowns.Log("roadblockHeavyCooldown  ");

		strategyCooldowns.Log("strategyCooldown        ");

		if (roadblockJoinTimers.isEnableds.current or reactToCooldownModes.current)
		{
			maxRBJoinDistances      .Log("maxRBJoinDistance       ");
			maxRBJoinElevationDeltas.Log("maxRBJoinElevationDeltas");
			maxRBJoinCounts         .Log("maxRBJoinCount          ");
		}

		roadblockJoinTimers.Log("roadblockJoinTimer      ");

		reactToCooldownModes.Log("reactToCooldownMode     ");
		reactToSpikesHits   .Log("reactToSpikesHit        ");

		heavy3TriggerCooldowns.Log("heavy3TriggerCooldown   ");
		heavy3AreBlockables   .Log("heavy3AreBlockable      ");

		heavy3LightVehicles   .Log("heavy3LightVehicle      ");
		heavy3HeavyVehicles   .Log("heavy3HeavyVehicle      ");
		heavy4LightVehicles   .Log("heavy4LightVehicle      ");
		heavy4HeavyVehicles   .Log("heavy4HeavyVehicle      ");
		leaderCrossVehicles   .Log("leaderCrossVehicle      ");
		leaderHenchmenVehicles.Log("leaderHenchmenVehicle   ");
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

		strategyCooldowns.SetToHeat(isRacing, heatLevel);

		maxRBJoinDistances      .SetToHeat(isRacing, heatLevel);
		maxRBJoinElevationDeltas.SetToHeat(isRacing, heatLevel);
		maxRBJoinCounts         .SetToHeat(isRacing, heatLevel);

		roadblockJoinTimers.SetToHeat(isRacing, heatLevel);

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

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
    }
}