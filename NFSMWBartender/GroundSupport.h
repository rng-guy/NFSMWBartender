#pragma once

#include <string>

#include "Globals.h"
#include "MemoryEditor.h"
#include "HeatParameters.h"



namespace GroundSupport
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<float> minRoadblockCooldowns  (8.f);  // seconds
	HeatParameters::Pair<float> maxRoadblockCooldowns  (12.f);
	HeatParameters::Pair<float> roadblockHeavyCooldowns(15.f);
	HeatParameters::Pair<float> strategyCooldowns      (10.f);
	HeatParameters::Pair<float> playerSpeedThresholds  (15.f); // kph
	
	HeatParameters::Pair<std::string> heavy3LightVehicles   ("copsuvl");
	HeatParameters::Pair<std::string> heavy3HeavyVehicles   ("copsuv");
	HeatParameters::Pair<std::string> heavy4LightVehicles   ("copsuvl");
	HeatParameters::Pair<std::string> heavy4HeavyVehicles   ("copsuv");
	HeatParameters::Pair<std::string> leaderCrossVehicles   ("copcross");
	HeatParameters::Pair<std::string> leaderHenchmenVehicles("copsporthench");

	// Conversions
	float roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current; // seconds
	float baseSpeedThreshold     = playerSpeedThresholds.current / 3.6f;                          // metres / second
	float jerkSpeedThreshold     = baseSpeedThreshold * .625f;





	// Auxiliary functions -----------------------------------------------------------------------------------------------------------------------------

	bool __fastcall IsValidHeavyStrategy
	(
		const address pursuit,
		const address strategy
	) {
		const int strategyType = *((int*)strategy);

		switch (strategyType)
		{
		case 3:
			return true;

		case 4:
			return (not *((address*)(pursuit + 0x84))); // active roadblock
		}

		return false;
	}



	bool __fastcall IsValidLeaderStrategy
	(
		const address pursuit,
		const address strategy
	) {
		if (*((int*)(pursuit + 0x164)) != 0) return false;

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



	template <typename T>
	void FindValidStrategies
	(
		const address         pursuit,
		const address         getNumStrategies,
		const address         getStrategies,
		const T&              validityPredicate,
		std::vector<address>& validStrategies
	) {
		static address (__thiscall* const GetSupportNode)(address) = (address (__thiscall*)(address))0x418EE0;

		const address supportNode = GetSupportNode(pursuit - 0x48);
		if (not supportNode) return;

		size_t  (__thiscall* const GetNumStrategies)(address)         = (size_t(__thiscall*)(address))         getNumStrategies;
		address (__thiscall* const GetStrategy)     (address, size_t) = (address(__thiscall*)(address, size_t))getStrategies;

		const size_t numStrategies = GetNumStrategies(supportNode);

		for (size_t strategyID = 0; strategyID < numStrategies; strategyID++)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not validityPredicate(pursuit, strategy)) continue;

			const int chance = *((int*)(strategy + 0x4));

			if (Globals::prng.Generate<int>(0, 100) < chance)
				validStrategies.push_back(strategy);
		}
	}



	address __fastcall GetRandomStrategy(const address pursuit) 
	{
		static std::vector<address> validStrategies;

		FindValidStrategies(pursuit, 0x403600, 0x4035E0, IsValidHeavyStrategy,  validStrategies);
		FindValidStrategies(pursuit, 0x403680, 0x403660, IsValidLeaderStrategy, validStrategies);

		if (not validStrategies.empty())
		{
			const size_t  index    = (validStrategies.size() > 1) ? Globals::prng.Generate<size_t>(0, validStrategies.size()) : 0;
			const address strategy = validStrategies[index];

			if constexpr (Globals::loggingEnabled)
			{
				const char* const strategyType = (IsValidHeavyStrategy(pursuit, strategy)) ? "HeavyStrategy" : "LeaderStrategy";
				Globals::logger.Log(pursuit, "[SUP] Requesting", strategyType, *((int*)strategy));
			}

			validStrategies.clear();

			return strategy;
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log(pursuit, "[SUP] Skipping Strategy request");

		return 0x0;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address strategySelectionEntrance = 0x41978D;
	constexpr address strategySelectionExit     = 0x41984E;

	__declspec(naked) void StrategySelection()
	{
		__asm
		{
			mov ecx, esi
			call GetRandomStrategy // ecx: pursuit
			test eax, eax
			je conclusion          // no Strategy selected

			mov ebx, eax
			mov edi, -0x1

			mov ecx, dword ptr [ebx + 0x8]
			mov dword ptr [esi + 0x208], ecx
			mov dword ptr [esi + 0x20C], 0x1

			mov ecx, esi
			mov edx, ebx
			call IsValidHeavyStrategy
			test al, al
			je leader // is LeaderStrategy

			mov dword ptr [esi + 0x194], ebx
			mov eax, dword ptr roadblockHeavyCooldowns.current
			mov dword ptr [esi + 0xC8], eax
			jmp conclusion // was HeavyStrategy

			leader:
			mov dword ptr [esi + 0x198], ebx

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



	constexpr address collapseCheckEntrance = 0x4437CD;
	constexpr address collapseCheckExit     = 0x4437D6;

	__declspec(naked) void CollapseCheck()
	{
		__asm
		{
			// Execute original code first
			fcom dword ptr [esp + 0x1C] // CollapseSpeed
			fnstsw ax
			test ah, 0x5

			fstp dword ptr [esp + 0x1C] // player speed

			jmp dword ptr collapseCheckExit
		}
	}



	constexpr address rammingCheckEntrance = 0x44385D;
	constexpr address rammingCheckExit     = 0x443865;

	__declspec(naked) void RammingCheck()
	{
		__asm
		{
			// Execute original code first
			mov eax, dword ptr [esi + 0x1DC]
			test eax, eax
			je conclusion // no active HeavyStrategy 3

			fld dword ptr [esp + 0x1C] // player speed

			mov al, byte ptr [esi + 0x280]
			test al, al
			jne jerk // AIPursuit::GetIsAJerk

			fcomp dword ptr baseSpeedThreshold
			jmp evaluation

			jerk:
			fcomp dword ptr jerkSpeedThreshold

			evaluation:
			fnstsw ax
			test ah, 0x1

			mov eax, dword ptr [esi + 0x1DC]

			conclusion:
			jmp dword ptr rammingCheckExit
		}
	}



	constexpr address onAttachedEntrance = 0x423F92;
	constexpr address onAttachedExit     = 0x423FC8;

	__declspec(naked) void OnAttached()
	{
		__asm
		{
			push dword ptr leaderCrossVehicles.current
			mov dword ptr [esi + 0x148], ebx
			call Globals::GetVaultKey

			push dword ptr heavy3HeavyVehicles.current
			mov ebx, eax
			call Globals::GetVaultKey

			push dword ptr heavy3LightVehicles.current
			mov ebp, eax
			call Globals::GetVaultKey

			push dword ptr leaderHenchmenVehicles.current
			mov dword ptr [esp + 0x30], eax
			call Globals::GetVaultKey

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

		// Other Heat parameters
		HeatParameters::Parse<float>(parser, "Heavy3:Behaviour",    {playerSpeedThresholds, 0.f});
		HeatParameters::Parse<float>(parser, "Strategies:Cooldown", {strategyCooldowns,     1.f});

		HeatParameters::Parse<std::string, std::string>(parser, "Heavy3:Vehicles", {heavy3LightVehicles}, {heavy3HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Heavy4:Vehicles", {heavy4LightVehicles}, {heavy4HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Leader:Vehicles", {leaderCrossVehicles}, {leaderHenchmenVehicles});

		// Code caves
		MemoryEditor::Write<float*>(&(minRoadblockCooldowns.current), {0x419548});

		MemoryEditor::DigCodeCave(StrategySelection, strategySelectionEntrance, strategySelectionExit);
		MemoryEditor::DigCodeCave(RoadblockCooldown, roadblockCooldownEntrance, roadblockCooldownExit);
        MemoryEditor::DigCodeCave(RequestCooldown,   requestCooldownEntrance,   requestCooldownExit);
		MemoryEditor::DigCodeCave(CollapseCheck,     collapseCheckEntrance,     collapseCheckExit);
		MemoryEditor::DigCodeCave(RammingCheck,      rammingCheckEntrance,      rammingCheckExit);
		MemoryEditor::DigCodeCave(OnAttached,        onAttachedEntrance,        onAttachedExit);
		MemoryEditor::DigCodeCave(OnDetached,        onDetachedEntrance,        onDetachedExit);
        MemoryEditor::DigCodeCave(LeaderSub,         leaderSubEntrance,         leaderSubExit);
		MemoryEditor::DigCodeCave(HenchmenSub,       henchmenSubEntrance,       henchmenSubExit);
        MemoryEditor::DigCodeCave(RhinoSelector,     rhinoSelectorEntrance,     rhinoSelectorExit);

		// Status flag
		featureEnabled = true;

		return true;
    }



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupport");

		// With logging disabled, the compiler optimises the string literals away
		HeatParameters::ReplaceInvalidVehicles("HeavyStrategy 3, light",   heavy3LightVehicles,    false);
		HeatParameters::ReplaceInvalidVehicles("HeavyStrategy 3, heavy",   heavy3HeavyVehicles,    false);
		HeatParameters::ReplaceInvalidVehicles("HeavyStrategy 4, light",   heavy4LightVehicles,    false);
		HeatParameters::ReplaceInvalidVehicles("HeavyStrategy 4, heavy",   heavy4HeavyVehicles,    false);
		HeatParameters::ReplaceInvalidVehicles("LeaderStrategy, Cross",    leaderCrossVehicles,    false);
		HeatParameters::ReplaceInvalidVehicles("LeaderStrategy, henchmen", leaderHenchmenVehicles, false);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		minRoadblockCooldowns  .SetToHeat(isRacing, heatLevel);
		maxRoadblockCooldowns  .SetToHeat(isRacing, heatLevel);
		roadblockHeavyCooldowns.SetToHeat(isRacing, heatLevel);
		strategyCooldowns      .SetToHeat(isRacing, heatLevel);
		playerSpeedThresholds  .SetToHeat(isRacing, heatLevel);

		heavy3LightVehicles   .SetToHeat(isRacing, heatLevel);
		heavy3HeavyVehicles   .SetToHeat(isRacing, heatLevel);
		heavy4LightVehicles   .SetToHeat(isRacing, heatLevel);
		heavy4HeavyVehicles   .SetToHeat(isRacing, heatLevel);
		leaderCrossVehicles   .SetToHeat(isRacing, heatLevel);
		leaderHenchmenVehicles.SetToHeat(isRacing, heatLevel);

		roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current;

		baseSpeedThreshold = playerSpeedThresholds.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [SUP] GroundSupport");

			Globals::logger.LogLongIndent("minRoadblockCooldown    ", minRoadblockCooldowns.current);
			Globals::logger.LogLongIndent("maxRoadblockCooldown    ", maxRoadblockCooldowns.current);
			Globals::logger.LogLongIndent("roadblockHeavyCooldown  ", roadblockHeavyCooldowns.current);
			Globals::logger.LogLongIndent("strategyCooldown        ", strategyCooldowns.current);
			Globals::logger.LogLongIndent("playerSpeedThreshold    ", playerSpeedThresholds.current);

			Globals::logger.LogLongIndent("heavy3LightVehicle      ", heavy3LightVehicles.current);
			Globals::logger.LogLongIndent("heavy3HeavyVehicle      ", heavy3HeavyVehicles.current);
			Globals::logger.LogLongIndent("heavy4LightVehicle      ", heavy4LightVehicles.current);
			Globals::logger.LogLongIndent("heavy4HeavyVehicle      ", heavy4HeavyVehicles.current);
			Globals::logger.LogLongIndent("leaderCrossVehicle      ", leaderCrossVehicles.current);
			Globals::logger.LogLongIndent("leaderHenchmenVehicle   ", leaderHenchmenVehicles.current);
		}
    }
}