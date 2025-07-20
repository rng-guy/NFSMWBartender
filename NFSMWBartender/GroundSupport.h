#pragma once

#include <Windows.h>
#include <algorithm>
#include <optional>
#include <string>
#include <array>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace GroundSupport
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	float minRoadblockCooldown   = 8.f;  // seconds
	float roadblockCooldownRange = 4.f;
	float roadblockHeavyCooldown = 15.f;
	float strategyCooldown       = 10.f;
	float maxStrategyDelay       = 10.f;

	const char* heavyRoadblockVehicle = "copsuv";
	const char* lightRoadblockVehicle = "copsuvl";
	const char* heavyRammingVehicle   = "copsuv";
	const char* lightRammingVehicle   = "copsuvl";
	const char* henchmenVehicle       = "copsporthench";
	const char* leaderVehicle         = "copcross";

	// Free-roam Heat levels
	std::array<float, Globals::maxHeatLevel> roamMinRoadblockCooldowns   = {};
	std::array<float, Globals::maxHeatLevel> roamRoadblockCooldownRanges = {};
	std::array<float, Globals::maxHeatLevel> roamRoadblockHeavyCooldowns = {};
	std::array<float, Globals::maxHeatLevel> roamStrategyCooldowns       = {};
	std::array<float, Globals::maxHeatLevel> roamMaxStrategyDelays       = {};
	
	std::array<std::string, Globals::maxHeatLevel> roamLightRammingVehicles   = {};
	std::array<std::string, Globals::maxHeatLevel> roamHeavyRammingVehicles   = {};
	std::array<std::string, Globals::maxHeatLevel> roamLightRoadblockVehicles = {};
	std::array<std::string, Globals::maxHeatLevel> roamHeavyRoadblockVehicles = {};
	std::array<std::string, Globals::maxHeatLevel> roamLeaderVehicles         = {};
	std::array<std::string, Globals::maxHeatLevel> roamHenchmenVehicles       = {};

	// Racing Heat levels
	std::array<float, Globals::maxHeatLevel> raceMinRoadblockCooldowns   = {};
	std::array<float, Globals::maxHeatLevel> raceRoadblockCooldownRanges = {};
	std::array<float, Globals::maxHeatLevel> raceRoadblockHeavyCooldowns = {};
	std::array<float, Globals::maxHeatLevel> raceStrategyCooldowns       = {};
	std::array<float, Globals::maxHeatLevel> raceMaxStrategyDelays       = {};

	std::array<std::string, Globals::maxHeatLevel> raceLightRammingVehicles   = {};
	std::array<std::string, Globals::maxHeatLevel> raceHeavyRammingVehicles   = {};
	std::array<std::string, Globals::maxHeatLevel> raceLightRoadblockVehicles = {};
	std::array<std::string, Globals::maxHeatLevel> raceHeavyRoadblockVehicles = {};
	std::array<std::string, Globals::maxHeatLevel> raceLeaderVehicles         = {};
	std::array<std::string, Globals::maxHeatLevel> raceHenchmenVehicles       = {};

	// Code caves
	constexpr address getRandom                 = 0x6ED200;
	constexpr address scaleRandom               = 0x402870;
	constexpr address clearSupportRequest       = 0x42BCF0;
	constexpr address getStringHash             = 0x5CC240;
	constexpr float   strategyCooldownThreshold = 0.f;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address roadblockCooldownEntrance = 0x419535;
	constexpr address roadblockCooldownExit     = 0x41954C;

	__declspec(naked) void RoadblockCooldown()
	{
		__asm
		{
			push roadblockCooldownRange
			call dword ptr getRandom
			mov ecx, eax
			call dword ptr scaleRandom
			fadd dword ptr minRoadblockCooldown

			jmp dword ptr roadblockCooldownExit
		}
	}



	constexpr address roadblockHeavyEntrance = 0x4197EC;
	constexpr address roadblockHeavyExit     = 0x4197F6;

	__declspec(naked) void RoadblockHeavy()
	{
		__asm
		{
			mov eax, roadblockHeavyCooldown
			mov [esi + 0xC8], eax

			jmp dword ptr roadblockHeavyExit
		}
	}



	constexpr address requestCooldownEntrance = 0x4196BF;
	constexpr address requestCooldownExit     = 0x4196E4;
	constexpr address requestCooldownSkip     = 0x41988A;

	__declspec(naked) void RequestCooldown()
	{
		__asm
		{
			fld dword ptr [esi + 0x210]
			fcomp dword ptr strategyCooldownThreshold
			fnstsw ax
			test ah, 0x1
			je skip // cooldown still ongoing

			push ebp
			lea ecx, [esi - 0x48]
			mov eax, strategyCooldown
			mov [esi + 0x210], eax

			jmp dword ptr requestCooldownExit

			skip:
			jmp dword ptr requestCooldownSkip
		}
	}



	constexpr address requestDelayEntrance = 0x419680;
	constexpr address requestDelayExit     = 0x419689;

	__declspec(naked) void RequestDelay()
	{
		__asm
		{
			cmp byte ptr [ecx + 0x20C], 0x2
			jne conclusion // is not the dreaded 2 flag

			fld dword ptr maxStrategyDelay
			fchs
			fcomp dword ptr [ecx + 0x210]
			fnstsw ax
			test ah, 0x1
			jne conclusion // request delay is below threshold

			push ecx
			call dword ptr clearSupportRequest
			pop ecx

			conclusion:
			// Execute original code and resume
			push esi
			mov esi, ecx
			mov al, [esi + 0xE8]

			jmp dword ptr requestDelayExit
		}
	}



	constexpr address onAttachedEntrance = 0x423F92;
	constexpr address onAttachedExit     = 0x423FC8;

	__declspec(naked) void OnAttached()
	{
		__asm
		{
			push leaderVehicle
			mov [esi + 0x148], ebx
			call dword ptr getStringHash

			push heavyRammingVehicle
			mov ebx, eax
			call dword ptr getStringHash

			push lightRammingVehicle
			mov ebp, eax
			call dword ptr getStringHash

			push henchmenVehicle
			mov [esp + 0x30], eax
			call dword ptr getStringHash

			jmp dword ptr onAttachedExit
		}
	}



	constexpr address onDetachedEntrance = 0x42B5F1;
	constexpr address onDetachedExit     = 0x42B5F6;

	__declspec(naked) void OnDetached()
	{
		__asm
		{
			push leaderVehicle
			jmp dword ptr onDetachedExit
		}
	}



	constexpr address leaderSubEntrance = 0x41F504;
	constexpr address leaderSubExit     = 0x41F50C;

	__declspec(naked) void LeaderSub()
	{
		__asm
		{
			mov ecx, leaderVehicle
			mov [esp + 0x24], ecx

			jmp dword ptr leaderSubExit
		}
	}



	constexpr address henchmenSubEntrance = 0x41F485;
	constexpr address henchmenSubExit     = 0x41F48A;

	__declspec(naked) void HenchmenSub()
	{
		__asm
		{
			mov eax, henchmenVehicle
			jmp dword ptr henchmenSubExit
		}
	}



	constexpr address rhinoSelectorEntrance = 0x41F1BC;
	constexpr address rhinoSelectorExit     = 0x41F1C8;

	__declspec(naked) void RhinoSelector()
	{
		__asm
		{
			mov eax, [esi]
			jl heavy // is "heavy" Rhino variant

			cmp byte ptr [eax], 0x3
			mov eax, lightRammingVehicle
			je conclusion  // is ramming strategy (3)
			mov eax, lightRoadblockVehicle
			jmp conclusion // is roadblock strategy (4)

			heavy:
			cmp byte ptr [eax], 0x3
			mov eax, heavyRammingVehicle
			je conclusion
			mov eax, heavyRoadblockVehicle

			conclusion:
			jmp dword ptr rhinoSelectorExit
		}
	}





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	std::array<bool, Globals::maxHeatLevel> ParseCooldowns
	(
		ConfigParser::Parser&                     parser,
		const std::string&                        format,
		std::array<float, Globals::maxHeatLevel>& minRoadblockCooldowns,
		std::array<float, Globals::maxHeatLevel>& roadblockCooldownRanges,
		std::array<float, Globals::maxHeatLevel>& roadblockHeavyCooldowns,
		const std::optional<float>                minRoadblockCooldown     = {},
		const std::optional<float>                maxRoadblockCooldown     = {},
		const std::optional<float>                roadblockHeavyCooldown   = {}
	) {
		std::array<float, Globals::maxHeatLevel> maxRoadblockCooldowns = {};

		const std::array<bool, Globals::maxHeatLevel> isValids = parser.ParseParameterTable
		(
			"Roadblocks:Timers",
			format,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minRoadblockCooldowns,   minRoadblockCooldown,   0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxRoadblockCooldowns,   maxRoadblockCooldown,   0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(roadblockHeavyCooldowns, roadblockHeavyCooldown, 0.f)
		);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			minRoadblockCooldowns[heatLevel - 1]   = std::min<float>(minRoadblockCooldowns[heatLevel - 1], maxRoadblockCooldowns[heatLevel - 1]);
			roadblockCooldownRanges[heatLevel - 1] = maxRoadblockCooldowns[heatLevel - 1] - minRoadblockCooldowns[heatLevel - 1];
		}

		return isValids;
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    bool Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configPathBasic + "Supports.ini")) return false;

		// Pursuit parameters
		ParseCooldowns
		(
			parser,
			Globals::configFormatRoam,
			roamMinRoadblockCooldowns,
			roamRoadblockCooldownRanges,
			roamRoadblockHeavyCooldowns,
			minRoadblockCooldown,
			minRoadblockCooldown + roadblockCooldownRange,
			roadblockHeavyCooldown
		);

		const std::array<bool, Globals::maxHeatLevel> isValids = ParseCooldowns
		(
			parser,
			Globals::configFormatRace,
			raceMinRoadblockCooldowns,
			raceRoadblockCooldownRanges,
			raceRoadblockHeavyCooldowns
		);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			if (not isValids[heatLevel - 1])
			{
				raceMinRoadblockCooldowns[heatLevel - 1]   = roamMinRoadblockCooldowns[heatLevel - 1];
				raceRoadblockCooldownRanges[heatLevel - 1] = roamRoadblockCooldownRanges[heatLevel - 1];
				raceRoadblockHeavyCooldowns[heatLevel - 1] = roamRoadblockHeavyCooldowns[heatLevel - 1];
			}
		}

		Globals::ParseHeatParameterPair<float, float>
		(
			parser,
			"Strategies:Timers",
			roamStrategyCooldowns,
			roamMaxStrategyDelays,
			raceStrategyCooldowns,
			raceMaxStrategyDelays,
			strategyCooldown,
			maxStrategyDelay,
			0.f,
			0.f
		);

		Globals::ParseHeatParameterPair<std::string, std::string>
		(
			parser,
			"Heavy:Ramming",
			roamLightRammingVehicles,
			roamHeavyRammingVehicles,
			raceLightRammingVehicles,
			raceHeavyRammingVehicles,
			lightRammingVehicle,
			heavyRammingVehicle
		);

		Globals::ParseHeatParameterPair<std::string, std::string>
		(
			parser,
			"Heavy:Roadblock",
			roamLightRoadblockVehicles,
			roamHeavyRoadblockVehicles,
			raceLightRoadblockVehicles,
			raceHeavyRoadblockVehicles,
			lightRoadblockVehicle,
			heavyRoadblockVehicle
		);

		Globals::ParseHeatParameterPair<std::string, std::string>
		(
			parser,
			"Leader:General",
			roamLeaderVehicles,
			roamHenchmenVehicles,
			raceLeaderVehicles,
			raceHenchmenVehicles,
			leaderVehicle,
			henchmenVehicle
		);

		// Code caves
		MemoryEditor::DigCodeCave(&RoadblockCooldown, roadblockCooldownEntrance, roadblockCooldownExit);
		MemoryEditor::DigCodeCave(&RoadblockHeavy,    roadblockHeavyEntrance,    roadblockHeavyExit);
        MemoryEditor::DigCodeCave(&RequestCooldown,   requestCooldownEntrance,   requestCooldownExit);
        MemoryEditor::DigCodeCave(&RequestDelay,      requestDelayEntrance,      requestDelayExit);
		MemoryEditor::DigCodeCave(&OnAttached,        onAttachedEntrance,        onAttachedExit);
		MemoryEditor::DigCodeCave(&OnDetached,        onDetachedEntrance,        onDetachedExit);
        MemoryEditor::DigCodeCave(&LeaderSub,         leaderSubEntrance,         leaderSubExit);
		MemoryEditor::DigCodeCave(&HenchmenSub,       henchmenSubEntrance,       henchmenSubExit);
        MemoryEditor::DigCodeCave(&RhinoSelector,     rhinoSelectorEntrance,     rhinoSelectorExit);

		return (featureEnabled = true);
    }



	void SetToHeat
	(
		const size_t heatLevel,
		const bool   isRacing
	) {
        if (not featureEnabled) return;

		if (isRacing)
		{
			minRoadblockCooldown   = raceMinRoadblockCooldowns[heatLevel - 1];
			roadblockCooldownRange = raceRoadblockCooldownRanges[heatLevel - 1];
			roadblockHeavyCooldown = raceRoadblockHeavyCooldowns[heatLevel - 1];
			strategyCooldown       = raceStrategyCooldowns[heatLevel - 1];
			maxStrategyDelay       = raceMaxStrategyDelays[heatLevel - 1];

			lightRammingVehicle   = raceLightRammingVehicles[heatLevel - 1].c_str();
			heavyRammingVehicle   = raceHeavyRammingVehicles[heatLevel - 1].c_str();
			lightRoadblockVehicle = raceLightRoadblockVehicles[heatLevel - 1].c_str();
			heavyRoadblockVehicle = raceHeavyRoadblockVehicles[heatLevel - 1].c_str();
			leaderVehicle         = raceLeaderVehicles[heatLevel - 1].c_str();
			henchmenVehicle       = raceHenchmenVehicles[heatLevel - 1].c_str();
		}
		else
		{
			minRoadblockCooldown   = roamMinRoadblockCooldowns[heatLevel - 1];
			roadblockCooldownRange = roamRoadblockCooldownRanges[heatLevel - 1];
			roadblockHeavyCooldown = roamRoadblockHeavyCooldowns[heatLevel - 1];
			strategyCooldown       = roamStrategyCooldowns[heatLevel - 1];
			maxStrategyDelay       = roamMaxStrategyDelays[heatLevel - 1];

			lightRammingVehicle   = roamLightRammingVehicles[heatLevel - 1].c_str();
			heavyRammingVehicle   = roamHeavyRammingVehicles[heatLevel - 1].c_str();
			lightRoadblockVehicle = roamLightRoadblockVehicles[heatLevel - 1].c_str();
			heavyRoadblockVehicle = roamHeavyRoadblockVehicles[heatLevel - 1].c_str();
			leaderVehicle         = roamLeaderVehicles[heatLevel - 1].c_str();
			henchmenVehicle       = roamHenchmenVehicles[heatLevel - 1].c_str();
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[SUP] GroundSupport");

			Globals::LogLongIndent("minRoadblockCooldown   :", minRoadblockCooldown);
			Globals::LogLongIndent("roadblockCooldownRange :", roadblockCooldownRange);
			Globals::LogLongIndent("roadblockHeavyCooldown :", roadblockHeavyCooldown);
			Globals::LogLongIndent("strategyCooldown       :", strategyCooldown);
			Globals::LogLongIndent("maxStrategyDelay       :", maxStrategyDelay);

			Globals::LogLongIndent("lightRammingVehicle    :", lightRammingVehicle);
			Globals::LogLongIndent("heavyRammingVehicle    :", heavyRammingVehicle);
			Globals::LogLongIndent("lightRoadblockVehicle  :", lightRoadblockVehicle);
			Globals::LogLongIndent("heavyRoadblockVehicle  :", heavyRoadblockVehicle);
			Globals::LogLongIndent("leaderVehicle          :", leaderVehicle);
			Globals::LogLongIndent("henchmenVehicle        :", henchmenVehicle);
		}
    }
}