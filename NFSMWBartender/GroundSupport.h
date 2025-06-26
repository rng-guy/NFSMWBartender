#pragma once

#include <Windows.h>
#include <algorithm>
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

	// General Heat levels
	std::array<float, Globals::maxHeatLevel> minRoadblockCooldowns   = {};
	std::array<float, Globals::maxHeatLevel> roadblockCooldownRanges = {};
	std::array<float, Globals::maxHeatLevel> roadblockHeavyCooldowns = {};
	std::array<float, Globals::maxHeatLevel> strategyCooldowns       = {};
	std::array<float, Globals::maxHeatLevel> maxStrategyDelays       = {};
	
	std::array<std::string, Globals::maxHeatLevel> heavyRoadblockVehicles = {};
	std::array<std::string, Globals::maxHeatLevel> lightRoadblockVehicles = {};
	std::array<std::string, Globals::maxHeatLevel> heavyRammingVehicles   = {};
	std::array<std::string, Globals::maxHeatLevel> lightRammingVehicles   = {};
	std::array<std::string, Globals::maxHeatLevel> henchmenVehicles       = {};
	std::array<std::string, Globals::maxHeatLevel> leaderVehicles         = {};

	// Code caves
	const address getRandom                 = 0x6ED200;
	const address scaleRandom               = 0x402870;
	const address clearSupportRequest       = 0x42BCF0;
	const address getStringHash             = 0x5CC240;
	const float   strategyCooldownThreshold = 0.f;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address roadblockCooldownEntrance = 0x419535;
	const address roadblockCooldownExit     = 0x41954C;

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



	const address roadblockHeavyEntrance = 0x4197EC;
	const address roadblockHeavyExit     = 0x4197F6;

	__declspec(naked) void RoadblockHeavy()
	{
		__asm
		{
			mov eax, roadblockHeavyCooldown
			mov [esi + 0xC8], eax

			jmp dword ptr roadblockHeavyExit
		}
	}



	const address requestCooldownEntrance = 0x4196BF;
	const address requestCooldownExit     = 0x4196E4;
	const address requestCooldownSkip     = 0x41988A;

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



	const address requestDelayEntrance = 0x419680;
	const address requestDelayExit     = 0x419689;

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



	const address onAttachedEntrance = 0x423F92;
	const address onAttachedExit     = 0x423FC8;

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



	const address onDetachedEntrance = 0x42B5F1;
	const address onDetachedExit     = 0x42B5F6;

	__declspec(naked) void OnDetached()
	{
		__asm
		{
			push leaderVehicle
			jmp dword ptr onDetachedExit
		}
	}



	const address leaderSubEntrance = 0x41F504;
	const address leaderSubExit     = 0x41F50C;

	__declspec(naked) void LeaderSub()
	{
		__asm
		{
			mov ecx, leaderVehicle
			mov [esp + 0x24], ecx

			jmp dword ptr leaderSubExit
		}
	}



	const address henchmenSubEntrance = 0x41F485;
	const address henchmenSubExit     = 0x41F48A;

	__declspec(naked) void HenchmenSub()
	{
		__asm
		{
			mov eax, henchmenVehicle
			jmp dword ptr henchmenSubExit
		}
	}



	const address rhinoSelectorEntrance = 0x41F1BC;
	const address rhinoSelectorExit     = 0x41F1C8;

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





    // State management -----------------------------------------------------------------------------------------------------------------------------

    void Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configBasicPath + "Supports.ini")) return;

		const float maxRoadblockCooldown = minRoadblockCooldown + roadblockCooldownRange;

		std::array<float, Globals::maxHeatLevel> maxRoadblockCooldowns = {};

		parser.ParseParameterTable
		(
			"Roadblocks:Timers",
			Globals::configFormat,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minRoadblockCooldowns,   minRoadblockCooldown,   0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxRoadblockCooldowns,   maxRoadblockCooldown,   0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(roadblockHeavyCooldowns, roadblockHeavyCooldown, 0.f)
		);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			minRoadblockCooldowns[heatLevel - 1]   = std::min(minRoadblockCooldowns[heatLevel - 1], maxRoadblockCooldowns[heatLevel - 1]);
			roadblockCooldownRanges[heatLevel - 1] = maxRoadblockCooldowns[heatLevel - 1] - minRoadblockCooldowns[heatLevel - 1];
		}

		parser.ParseParameterTable
		(
			"Strategies:Timers",
			Globals::configFormat,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(strategyCooldowns, strategyCooldown, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxStrategyDelays, maxStrategyDelay, 0.f)
		);

		parser.ParseParameterTable
		(
			"Heavy:Ramming",
			Globals::configFormat,
			ConfigParser::FormatParameter<std::string, Globals::maxHeatLevel>(lightRammingVehicles, lightRammingVehicle),
			ConfigParser::FormatParameter<std::string, Globals::maxHeatLevel>(heavyRammingVehicles, heavyRammingVehicle)
		);

		parser.ParseParameterTable
		(
			"Heavy:Roadblock",
			Globals::configFormat,
			ConfigParser::FormatParameter<std::string, Globals::maxHeatLevel>(lightRoadblockVehicles, lightRoadblockVehicle),
			ConfigParser::FormatParameter<std::string, Globals::maxHeatLevel>(heavyRoadblockVehicles, heavyRoadblockVehicle)
		);

		parser.ParseParameterTable
		(
			"Leader:General",
			Globals::configFormat,
			ConfigParser::FormatParameter<std::string, Globals::maxHeatLevel>(leaderVehicles,   leaderVehicle),
			ConfigParser::FormatParameter<std::string, Globals::maxHeatLevel>(henchmenVehicles, henchmenVehicle)
		);

		MemoryEditor::DigCodeCave(&RoadblockCooldown, roadblockCooldownEntrance, roadblockCooldownExit);
		MemoryEditor::DigCodeCave(&RoadblockHeavy,    roadblockHeavyEntrance,    roadblockHeavyExit);
        MemoryEditor::DigCodeCave(&RequestCooldown,   requestCooldownEntrance,   requestCooldownExit);
        MemoryEditor::DigCodeCave(&RequestDelay,      requestDelayEntrance,      requestDelayExit);
		MemoryEditor::DigCodeCave(&OnAttached,        onAttachedEntrance,        onAttachedExit);
		MemoryEditor::DigCodeCave(&OnDetached,        onDetachedEntrance,        onDetachedExit);
        MemoryEditor::DigCodeCave(&LeaderSub,         leaderSubEntrance,         leaderSubExit);
		MemoryEditor::DigCodeCave(&HenchmenSub,       henchmenSubEntrance,       henchmenSubExit);
        MemoryEditor::DigCodeCave(&RhinoSelector,     rhinoSelectorEntrance,     rhinoSelectorExit);

        featureEnabled = true;
    }



    void SetToHeat(const size_t heatLevel)
    {
        if (not featureEnabled) return;

		minRoadblockCooldown   = minRoadblockCooldowns[heatLevel - 1];
		roadblockCooldownRange = roadblockCooldownRanges[heatLevel - 1];
		roadblockHeavyCooldown = roadblockHeavyCooldowns[heatLevel - 1];
        strategyCooldown       = strategyCooldowns[heatLevel - 1];
        maxStrategyDelay       = maxStrategyDelays[heatLevel - 1];

        heavyRoadblockVehicle = heavyRoadblockVehicles[heatLevel - 1].c_str();
        lightRoadblockVehicle = lightRoadblockVehicles[heatLevel - 1].c_str();
        heavyRammingVehicle   = heavyRammingVehicles[heatLevel - 1].c_str();
        lightRammingVehicle   = lightRammingVehicles[heatLevel - 1].c_str();
        henchmenVehicle       = henchmenVehicles[heatLevel - 1].c_str();
        leaderVehicle         = leaderVehicles[heatLevel - 1].c_str();
    }
}