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
	HeatParameters::Pair<bool> rivalStrategiesEnableds(true);
	HeatParameters::Pair<bool> rivalRoadblocksEnableds(true);

	HeatParameters::Pair<float> minRoadblockCooldowns  (8.f);  // seconds
	HeatParameters::Pair<float> maxRoadblockCooldowns  (12.f);
	HeatParameters::Pair<float> roadblockHeavyCooldowns(15.f);
	HeatParameters::Pair<float> strategyCooldowns      (10.f);
	
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
			return true;

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

		for (size_t strategyID = 0; strategyID < numStrategies; strategyID++)
		{
			const address strategy = GetStrategy(supportNode, strategyID);
			if (not IsStrategyAvailable(pursuit, strategy)) continue;

			const int chance = *((int*)(strategy + 0x4));

			if (Globals::prng.Generate<int>(0, 100) < chance)
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
			*((address*)(pursuit + 0x194)) = strategy;                        // HeavyStrategy
			*((float*)(pursuit + 0xC8))    = roadblockHeavyCooldowns.current; // roadblock CD
		}
		else *((address*)(pursuit + 0x198)) = strategy; // LeaderStrategy
	}



	bool __fastcall SetRandomStrategy(const address pursuit) 
	{
		static std::vector<address> candidateStrategies;

		MarshalStrategies<0x403600, 0x4035E0, IsHeavyStrategyAvailable>(pursuit, candidateStrategies);
		const size_t numHeavyStrategies = candidateStrategies.size();
		MarshalStrategies<0x403680, 0x403660, IsLeaderStrategyAvailable>(pursuit, candidateStrategies);

		if (not candidateStrategies.empty())
		{
			const size_t index = (candidateStrategies.size() > 1) ? Globals::prng.Generate<size_t>(0, candidateStrategies.size()) : 0;

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
			Globals::logger.Log(pursuit, "[SUP] Strategy request failed");

		return false;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

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



	constexpr address requestCooldownEntrance = 0x4196D6;
	constexpr address requestCooldownExit     = 0x4196E4;

	__declspec(naked) void RequestCooldown()
	{
		static constexpr address requestCooldownSkip = 0x41988A;

		__asm
		{
			mov eax, dword ptr strategyCooldowns.current
			mov dword ptr [esi + 0x210], eax

			cmp byte ptr rivalStrategiesEnableds.current, 0x1
			je conclusion // no rival discrimination

			mov ecx, esi
			mov eax, dword ptr [esi]
			call dword ptr [eax + 0x8C]
			test eax, eax
			jne conclusion // is player pursuit

			jmp dword ptr requestCooldownSkip // was rival pursuit

			conclusion:
			// Execute original code and resume
			push ebp
			lea ecx, dword ptr [esi - 0x48]

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
		HeatParameters::Parse<bool, bool>(parser, "Supports:Rivals",     {rivalRoadblocksEnableds}, {rivalStrategiesEnableds});
		HeatParameters::Parse<float>     (parser, "Strategies:Cooldown", {strategyCooldowns, 1.f});

		HeatParameters::Parse<std::string, std::string>(parser, "Heavy3:Vehicles", {heavy3LightVehicles}, {heavy3HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Heavy4:Vehicles", {heavy4LightVehicles}, {heavy4HeavyVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Leader:Vehicles", {leaderCrossVehicles}, {leaderHenchmenVehicles});

		// Code caves
		MemoryEditor::Write<float*>(&(minRoadblockCooldowns.current), {0x419548});

		MemoryEditor::DigCodeCave(StrategySelection, strategySelectionEntrance, strategySelectionExit);
		MemoryEditor::DigCodeCave(RoadblockCooldown, roadblockCooldownEntrance, roadblockCooldownExit);
		MemoryEditor::DigCodeCave(RivalRoadblock,    rivalRoadblockEntrance,    rivalRoadblockExit);
        MemoryEditor::DigCodeCave(RequestCooldown,   requestCooldownEntrance,   requestCooldownExit);
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
		heavy3LightVehicles   .Validate(Globals::VehicleClass::CAR, "HeavyStrategy 3, light");
		heavy3HeavyVehicles   .Validate(Globals::VehicleClass::CAR, "HeavyStrategy 3, heavy");
		heavy4LightVehicles   .Validate(Globals::VehicleClass::CAR, "HeavyStrategy 4, light");
		heavy4HeavyVehicles   .Validate(Globals::VehicleClass::CAR, "HeavyStrategy 4, heavy");
		leaderCrossVehicles   .Validate(Globals::VehicleClass::CAR, "LeaderStrategy, Cross");
		leaderHenchmenVehicles.Validate(Globals::VehicleClass::CAR, "LeaderStrategy, henchmen");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		rivalStrategiesEnableds.SetToHeat(isRacing, heatLevel);
		rivalRoadblocksEnableds.SetToHeat(isRacing, heatLevel);

		minRoadblockCooldowns  .SetToHeat(isRacing, heatLevel);
		maxRoadblockCooldowns  .SetToHeat(isRacing, heatLevel);
		roadblockHeavyCooldowns.SetToHeat(isRacing, heatLevel);
		strategyCooldowns      .SetToHeat(isRacing, heatLevel);

		heavy3LightVehicles   .SetToHeat(isRacing, heatLevel);
		heavy3HeavyVehicles   .SetToHeat(isRacing, heatLevel);
		heavy4LightVehicles   .SetToHeat(isRacing, heatLevel);
		heavy4HeavyVehicles   .SetToHeat(isRacing, heatLevel);
		leaderCrossVehicles   .SetToHeat(isRacing, heatLevel);
		leaderHenchmenVehicles.SetToHeat(isRacing, heatLevel);

		roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [SUP] GroundSupport");

			Globals::logger.LogLongIndent("rivalStrategiesEnabled  ", (rivalStrategiesEnableds.current) ? "true" : "false");
			Globals::logger.LogLongIndent("rivalRoadblocksEnabled  ", (rivalRoadblocksEnableds.current) ? "true" : "false");

			Globals::logger.LogLongIndent("roadblockCooldown       ", minRoadblockCooldowns.current, "to", maxRoadblockCooldowns.current);
			Globals::logger.LogLongIndent("roadblockHeavyCooldown  ", roadblockHeavyCooldowns.current);
			Globals::logger.LogLongIndent("strategyCooldown        ", strategyCooldowns.current);

			Globals::logger.LogLongIndent("heavy3LightVehicle      ", heavy3LightVehicles.current);
			Globals::logger.LogLongIndent("heavy3HeavyVehicle      ", heavy3HeavyVehicles.current);
			Globals::logger.LogLongIndent("heavy4LightVehicle      ", heavy4LightVehicles.current);
			Globals::logger.LogLongIndent("heavy4HeavyVehicle      ", heavy4HeavyVehicles.current);
			Globals::logger.LogLongIndent("leaderCrossVehicle      ", leaderCrossVehicles.current);
			Globals::logger.LogLongIndent("leaderHenchmenVehicle   ", leaderHenchmenVehicles.current);
		}
    }
}