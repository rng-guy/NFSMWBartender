#pragma once

#include <Windows.h>
#include <optional>
#include <string>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace GroundSupport
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	float minRoadblockCooldown   = 8.f;  // seconds
	float maxRoadblockCooldown   = 12.f;
	float roadblockHeavyCooldown = 15.f;
	float strategyCooldown       = 10.f;
	float maxStrategyDelay       = 10.f;

	const char* heavyRoadblockVehicle = "copsuv";
	const char* lightRoadblockVehicle = "copsuvl";
	const char* heavyRammingVehicle   = "copsuv";
	const char* lightRammingVehicle   = "copsuvl";
	const char* henchmenVehicle       = "copsporthench";
	const char* leaderVehicle         = "copcross";

	// Heat levels
	Globals::HeatParametersPair<float> minRoadblockCooldowns;
	Globals::HeatParametersPair<float> maxRoadblockCooldowns;
	Globals::HeatParametersPair<float> roadblockHeavyCooldowns;
	Globals::HeatParametersPair<float> strategyCooldowns;
	Globals::HeatParametersPair<float> maxStrategyDelays;
	
	Globals::HeatParametersPair<std::string> lightRammingVehicles;
	Globals::HeatParametersPair<std::string> heavyRammingVehicles;
	Globals::HeatParametersPair<std::string> lightRoadblockVehicles;
	Globals::HeatParametersPair<std::string> heavyRoadblockVehicles;
	Globals::HeatParametersPair<std::string> leaderVehicles;
	Globals::HeatParametersPair<std::string> henchmenVehicles;

	// Conversions
	float roadblockCooldownRange = maxRoadblockCooldown - minRoadblockCooldown;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

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



	constexpr address roadblockHeavyEntrance = 0x4197EC;
	constexpr address roadblockHeavyExit     = 0x4197F6;

	__declspec(naked) void RoadblockHeavy()
	{
		__asm
		{
			mov eax, dword ptr roadblockHeavyCooldown
			mov dword ptr [esi + 0xC8], eax

			jmp dword ptr roadblockHeavyExit
		}
	}



	constexpr address requestCooldownEntrance = 0x4196DA;
	constexpr address requestCooldownExit     = 0x4196E4;

	__declspec(naked) void RequestCooldown()
	{
		__asm
		{
			mov eax, dword ptr strategyCooldown
			mov dword ptr [esi + 0x210], eax

			jmp dword ptr requestCooldownExit
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
			push dword ptr leaderVehicle
			mov dword ptr [esi + 0x148], ebx
			call Globals::GetStringHash

			push dword ptr heavyRammingVehicle
			mov ebx, eax
			call Globals::GetStringHash

			push dword ptr lightRammingVehicle
			mov ebp, eax
			call Globals::GetStringHash

			push dword ptr henchmenVehicle
			mov dword ptr [esp + 0x30], eax
			call Globals::GetStringHash

			jmp dword ptr onAttachedExit
		}
	}



	constexpr address onDetachedEntrance = 0x42B5F1;
	constexpr address onDetachedExit     = 0x42B5F6;

	__declspec(naked) void OnDetached()
	{
		__asm
		{
			push dword ptr leaderVehicle

			jmp dword ptr onDetachedExit
		}
	}



	constexpr address leaderSubEntrance = 0x41F504;
	constexpr address leaderSubExit     = 0x41F50C;

	__declspec(naked) void LeaderSub()
	{
		__asm
		{
			mov ecx, dword ptr leaderVehicle
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
			mov eax, dword ptr henchmenVehicle

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
			mov eax, dword ptr lightRammingVehicle
			je conclusion  // is ramming strategy (3)
			mov eax, dword ptr lightRoadblockVehicle
			jmp conclusion // is roadblock strategy (4)

			heavy:
			cmp byte ptr [eax], 0x3
			mov eax, dword ptr heavyRammingVehicle
			je conclusion
			mov eax, dword ptr heavyRoadblockVehicle

			conclusion:
			jmp dword ptr rhinoSelectorExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    bool Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configPathBasic + "Supports.ini")) return false;

		// Roadblock timers
		Globals::ParseHeatParameters<float, float, float>
		(
			parser,
			"Roadblocks:Timers",
			{minRoadblockCooldowns,   minRoadblockCooldown,   0.f},
			{maxRoadblockCooldowns,   maxRoadblockCooldown,   0.f},
			{roadblockHeavyCooldowns, roadblockHeavyCooldown, 0.f}
		);

		Globals::CheckIntervalValues<float>(minRoadblockCooldowns, maxRoadblockCooldowns);

		// Other pursuit parameters
		Globals::ParseHeatParameters<float, float>
		(
			parser,
			"Strategies:Timers",
			{strategyCooldowns, strategyCooldown, 0.f},
			{maxStrategyDelays, maxStrategyDelay, 0.f}
		);

		Globals::ParseHeatParameters<std::string, std::string>
		(
			parser,
			"Heavy:Ramming",
			{lightRammingVehicles, lightRammingVehicle},
			{heavyRammingVehicles, heavyRammingVehicle}
		);

		Globals::ParseHeatParameters<std::string, std::string>
		(
			parser,
			"Heavy:Roadblock",
			{lightRoadblockVehicles, lightRoadblockVehicle},
			{heavyRoadblockVehicles, heavyRoadblockVehicle}
		);

		Globals::ParseHeatParameters<std::string, std::string>
		(
			parser,
			"Leader:General",
			{leaderVehicles,   leaderVehicle},
			{henchmenVehicles, henchmenVehicle}
		);

		// Code caves
		MemoryEditor::Write<float*>(&minRoadblockCooldown, 0x419548);

		MemoryEditor::DigCodeCave(RoadblockCooldown, roadblockCooldownEntrance, roadblockCooldownExit);
		MemoryEditor::DigCodeCave(RoadblockHeavy,    roadblockHeavyEntrance,    roadblockHeavyExit);
        MemoryEditor::DigCodeCave(RequestCooldown,   requestCooldownEntrance,   requestCooldownExit);
        MemoryEditor::DigCodeCave(RequestDelay,      requestDelayEntrance,      requestDelayExit);
		MemoryEditor::DigCodeCave(OnAttached,        onAttachedEntrance,        onAttachedExit);
		MemoryEditor::DigCodeCave(OnDetached,        onDetachedEntrance,        onDetachedExit);
        MemoryEditor::DigCodeCave(LeaderSub,         leaderSubEntrance,         leaderSubExit);
		MemoryEditor::DigCodeCave(HenchmenSub,       henchmenSubEntrance,       henchmenSubExit);
        MemoryEditor::DigCodeCave(RhinoSelector,     rhinoSelectorEntrance,     rhinoSelectorExit);

		return (featureEnabled = true);
    }



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		minRoadblockCooldown   = minRoadblockCooldowns  (isRacing, heatLevel);
		maxRoadblockCooldown   = maxRoadblockCooldowns  (isRacing, heatLevel);
		roadblockHeavyCooldown = roadblockHeavyCooldowns(isRacing, heatLevel);
		strategyCooldown       = strategyCooldowns      (isRacing, heatLevel);
		maxStrategyDelay       = maxStrategyDelays      (isRacing, heatLevel);

		lightRammingVehicle   = lightRammingVehicles  (isRacing, heatLevel).c_str();
		heavyRammingVehicle   = heavyRammingVehicles  (isRacing, heatLevel).c_str();
		lightRoadblockVehicle = lightRoadblockVehicles(isRacing, heatLevel).c_str();
		heavyRoadblockVehicle = heavyRoadblockVehicles(isRacing, heatLevel).c_str();
		leaderVehicle         = leaderVehicles        (isRacing, heatLevel).c_str();
		henchmenVehicle       = henchmenVehicles      (isRacing, heatLevel).c_str();

		roadblockCooldownRange = maxRoadblockCooldown - minRoadblockCooldown;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[SUP] GroundSupport");

			Globals::LogLongIndent("minRoadblockCooldown   :", minRoadblockCooldown);
			Globals::LogLongIndent("maxRoadblockCooldown   :", maxRoadblockCooldown);
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