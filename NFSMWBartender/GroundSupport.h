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
	HeatParameters::Pair<float> strategyResetTimers    (20.f);
	HeatParameters::Pair<float> playerSpeedThresholds  (15.f); // kph
	
	HeatParameters::Pair<std::string> lightRammingVehicles  ("copsuvl");
	HeatParameters::Pair<std::string> heavyRammingVehicles  ("copsuv");
	HeatParameters::Pair<std::string> lightRoadblockVehicles("copsuvl");
	HeatParameters::Pair<std::string> heavyRoadblockVehicles("copsuv");
	HeatParameters::Pair<std::string> leaderVehicles        ("copcross");
	HeatParameters::Pair<std::string> henchmenVehicles      ("copsporthench");

	// Conversions
	float roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current; // seconds
	float strategyResetThreshold = strategyCooldowns.current     - strategyResetTimers.current;
	float baseSpeedThreshold     = playerSpeedThresholds.current / 3.6f;                          // metres / second
	float jerkSpeedThreshold     = baseSpeedThreshold * .625f;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address roadblockHeavyEntrance = 0x4197EC;
	constexpr address roadblockHeavyExit     = 0x4197F6;

	__declspec(naked) void RoadblockHeavy()
	{
		__asm
		{
			mov eax, dword ptr roadblockHeavyCooldowns.current
			mov dword ptr [esi + 0xC8], eax

			jmp dword ptr roadblockHeavyExit
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



	constexpr address heavyChanceEntrance = 0x4197BB;
	constexpr address heavyChanceExit     = 0x4197C3;

	__declspec(naked) void HeavyChance()
	{
		static constexpr address getHeavyStrategy = 0x4035E0;

		__asm
		{
			call dword ptr getHeavyStrategy

			cmp byte ptr [eax], 0x3
			je included // is HeavyStrategy 3

			mov edx, dword ptr [esi + 0x84]
			test edx, edx
			je included // no active roadblock

			sub edi, 0x0
			jmp conclusion

			included:
			sub edi, dword ptr [eax + 0x4]

			conclusion:
			jmp dword ptr heavyChanceExit
		}
	}



	constexpr address requestDelayEntrance = 0x419680;
	constexpr address requestDelayExit     = 0x419689;

	__declspec(naked) void RequestDelay()
	{
		static constexpr address clearSupportRequest = 0x42BCF0;

		__asm
		{
			cmp byte ptr [ecx + 0x20C], 0x2
			jne conclusion // is not the dreaded 2 flag

			fld dword ptr strategyResetThreshold
			fcomp dword ptr [ecx + 0x210]
			fnstsw ax
			test ah, 0x1
			jne conclusion // Strategy cooldown below threshold

			push ecx
			call dword ptr clearSupportRequest
			pop ecx

			conclusion:
			// Execute original code and resume
			push esi
			mov esi, ecx
			mov al, byte ptr [esi + 0xE8]

			jmp dword ptr requestDelayExit
		}
	}



	constexpr address onAttachedEntrance = 0x423F92;
	constexpr address onAttachedExit     = 0x423FC8;

	__declspec(naked) void OnAttached()
	{
		__asm
		{
			push dword ptr leaderVehicles.current
			mov dword ptr [esi + 0x148], ebx
			call Globals::GetVaultKey

			push dword ptr heavyRammingVehicles.current
			mov ebx, eax
			call Globals::GetVaultKey

			push dword ptr lightRammingVehicles.current
			mov ebp, eax
			call Globals::GetVaultKey

			push dword ptr henchmenVehicles.current
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
			push dword ptr leaderVehicles.current

			jmp dword ptr onDetachedExit
		}
	}



	constexpr address leaderSubEntrance = 0x41F504;
	constexpr address leaderSubExit     = 0x41F50C;

	__declspec(naked) void LeaderSub()
	{
		__asm
		{
			mov ecx, dword ptr leaderVehicles.current
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
			mov eax, dword ptr henchmenVehicles.current

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
			mov eax, dword ptr lightRammingVehicles.current
			je conclusion  // is ramming strategy (3)
			mov eax, dword ptr lightRoadblockVehicles.current
			jmp conclusion // is roadblock strategy (4)

			heavy:
			cmp byte ptr [eax], 0x3
			mov eax, dword ptr heavyRammingVehicles.current
			je conclusion
			mov eax, dword ptr heavyRoadblockVehicles.current

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
			"Roadblocks:Timers",
			{minRoadblockCooldowns,   0.f},
			{maxRoadblockCooldowns,   0.f},
			{roadblockHeavyCooldowns, 0.f}
		);

		HeatParameters::CheckIntervals<float>(minRoadblockCooldowns, maxRoadblockCooldowns);

		// Other pursuit parameters
		HeatParameters::Parse<float>       (parser, "Heavy:Behaviour",   {playerSpeedThresholds, 0.f});
		HeatParameters::Parse<float, float>(parser, "Strategies:Timers", {strategyCooldowns,     0.f}, {strategyResetTimers, 0.f});

		HeatParameters::Parse<std::string, std::string>(parser, "Heavy:Ramming",   {lightRammingVehicles},   {heavyRammingVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Heavy:Roadblock", {lightRoadblockVehicles}, {heavyRoadblockVehicles});
		HeatParameters::Parse<std::string, std::string>(parser, "Leader:General",  {leaderVehicles},         {henchmenVehicles});

		// Code caves
		MemoryEditor::Write<float*>(&(minRoadblockCooldowns.current), {0x419548});
		MemoryEditor::WriteToAddressRange(0x90, 0x4197A9, 0x4197AB); // roadblocks blocking HeavyStrategy 3

		MemoryEditor::DigCodeCave(RoadblockCooldown, roadblockCooldownEntrance, roadblockCooldownExit);
        MemoryEditor::DigCodeCave(RequestCooldown,   requestCooldownEntrance,   requestCooldownExit);
		MemoryEditor::DigCodeCave(RoadblockHeavy,    roadblockHeavyEntrance,    roadblockHeavyExit);
		MemoryEditor::DigCodeCave(CollapseCheck,     collapseCheckEntrance,     collapseCheckExit);
		MemoryEditor::DigCodeCave(RammingCheck,      rammingCheckEntrance,      rammingCheckExit);
        MemoryEditor::DigCodeCave(RequestDelay,      requestDelayEntrance,      requestDelayExit);
		MemoryEditor::DigCodeCave(HeavyChance,       heavyChanceEntrance,       heavyChanceExit);
		MemoryEditor::DigCodeCave(OnAttached,        onAttachedEntrance,        onAttachedExit);
		MemoryEditor::DigCodeCave(OnDetached,        onDetachedEntrance,        onDetachedExit);
        MemoryEditor::DigCodeCave(LeaderSub,         leaderSubEntrance,         leaderSubExit);
		MemoryEditor::DigCodeCave(HenchmenSub,       henchmenSubEntrance,       henchmenSubExit);
        MemoryEditor::DigCodeCave(RhinoSelector,     rhinoSelectorEntrance,     rhinoSelectorExit);

		return (featureEnabled = true);
    }



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SUP] GroundSupport");

		// With logging disabled, the compiler optimises the string literals away
		HeatParameters::ReplaceInvalidVehicles("Light ram. SUVs", lightRammingVehicles,   false);
		HeatParameters::ReplaceInvalidVehicles("Heavy ram. SUVs", heavyRammingVehicles,   false);
		HeatParameters::ReplaceInvalidVehicles("Light rdb. SUVs", lightRoadblockVehicles, false);
		HeatParameters::ReplaceInvalidVehicles("Heavy rdb. SUVs", heavyRoadblockVehicles, false);
		HeatParameters::ReplaceInvalidVehicles("Leader",          leaderVehicles,         false);
		HeatParameters::ReplaceInvalidVehicles("Henchmen",        henchmenVehicles,       false);
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
		strategyResetTimers    .SetToHeat(isRacing, heatLevel);
		playerSpeedThresholds  .SetToHeat(isRacing, heatLevel);

		lightRammingVehicles  .SetToHeat(isRacing, heatLevel);
		heavyRammingVehicles  .SetToHeat(isRacing, heatLevel);
		lightRoadblockVehicles.SetToHeat(isRacing, heatLevel);
		heavyRoadblockVehicles.SetToHeat(isRacing, heatLevel);
		leaderVehicles        .SetToHeat(isRacing, heatLevel);
		henchmenVehicles      .SetToHeat(isRacing, heatLevel);

		roadblockCooldownRange = maxRoadblockCooldowns.current - minRoadblockCooldowns.current;
		strategyResetThreshold = strategyCooldowns.current     - strategyResetTimers.current;
		baseSpeedThreshold     = playerSpeedThresholds.current / 3.6f;
		jerkSpeedThreshold     = baseSpeedThreshold * .625f;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [SUP] GroundSupport");

			Globals::logger.LogLongIndent("minRoadblockCooldown    ", minRoadblockCooldowns.current);
			Globals::logger.LogLongIndent("maxRoadblockCooldown    ", maxRoadblockCooldowns.current);
			Globals::logger.LogLongIndent("roadblockHeavyCooldown  ", roadblockHeavyCooldowns.current);
			Globals::logger.LogLongIndent("strategyCooldown        ", strategyCooldowns.current);
			Globals::logger.LogLongIndent("strategyResetTimer      ", strategyResetTimers.current);
			Globals::logger.LogLongIndent("playerSpeedThreshold    ", playerSpeedThresholds.current);

			Globals::logger.LogLongIndent("lightRammingVehicle     ", lightRammingVehicles.current);
			Globals::logger.LogLongIndent("heavyRammingVehicle     ", heavyRammingVehicles.current);
			Globals::logger.LogLongIndent("lightRoadblockVehicle   ", lightRoadblockVehicles.current);
			Globals::logger.LogLongIndent("heavyRoadblockVehicle   ", heavyRoadblockVehicles.current);
			Globals::logger.LogLongIndent("leaderVehicle           ", leaderVehicles.current);
			Globals::logger.LogLongIndent("henchmenVehicle         ", henchmenVehicles.current);
		}
    }
}