#pragma once

#include <Windows.h>
#include <array>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace PursuitBar 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	float maxBustDistance = 15.f; // metres
	float evadeTimer      = 7.f;  // seconds
	float bustTimer       = 5.f;  // seconds
	
	// Free-roam Heat levels
	std::array<float, Globals::maxHeatLevel> roamMaxBustDistances = {};
	std::array<float, Globals::maxHeatLevel> roamEvadeTimers      = {};
	std::array<float, Globals::maxHeatLevel> roamBustTimers       = {};

	// Racing Heat levels
	std::array<float, Globals::maxHeatLevel> raceMaxBustDistances = {};
	std::array<float, Globals::maxHeatLevel> raceEvadeTimers      = {};
	std::array<float, Globals::maxHeatLevel> raceBustTimers       = {};

	// Code caves
	const float oneHalf                = .5f;
	const float evadeStateThreshold    = 0.f;
	const float obstructedBustDistance = 0.f;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address maxBustDistanceEntrance = 0x444483;
	const address maxBustDistanceExit     = 0x44448B;

	__declspec(naked) void MaxBustDistance()
	{
		__asm
		{
			fld dword ptr [esi + 0x168] // EVADE progress
			fcomp dword ptr evadeStateThreshold
			fnstsw ax
			test ah, ah
			je obstructed // green EVADE bar is filling

			fld dword ptr maxBustDistance
			jmp conclusion // is not in EVADE state

			obstructed:
			fld dword ptr obstructedBustDistance

			conclusion:
			// Execute original code and resume
			test bl, bl

			jmp dword ptr maxBustDistanceExit
		}
	}



	const address evadeBarEntrance = 0x444A38;
	const address evadeBarExit     = 0x444A3E;

	__declspec(naked) void EvadeBar()
	{
		__asm
		{
			fdiv dword ptr evadeTimer
			fmul dword ptr oneHalf

			jmp dword ptr evadeBarExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    void Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configPathBasic + "Others.ini")) return;

		// Pursuit parameters
		Globals::ParseHeatParameterPair<float, float>
		(
			parser,
			"Busting:General",
			roamMaxBustDistances,
			roamBustTimers,
			raceMaxBustDistances,
			raceBustTimers,
			maxBustDistance,
			bustTimer,
			0.f,
			.001f
		);

		Globals::ParseHeatParameter<float>
		(
			parser,
			"Evading:Timer",
			roamEvadeTimers,
			raceEvadeTimers,
			evadeTimer,
			.001f
		);

		// Code caves
        MemoryEditor::Write<WORD>  (0x35D8,      0x40AED9); // opcode fdiv m
        MemoryEditor::Write<float*>(&bustTimer,  0x4445CE,  0x40AEDB);
        MemoryEditor::Write<float*>(&evadeTimer, 0x4448E6,  0x444802,  0x4338F8);

        MemoryEditor::DigCodeCave(&MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);
        MemoryEditor::DigCodeCave(&EvadeBar,        evadeBarEntrance,        evadeBarExit);

        featureEnabled = true;
    }



    void SetToHeat
	(
		const size_t heatLevel,
		const bool   isRacing
	) {
        if (not featureEnabled) return;

		if (isRacing)
		{ 
			maxBustDistance = raceMaxBustDistances[heatLevel - 1];
			bustTimer       = raceBustTimers[heatLevel - 1];
			evadeTimer      = raceEvadeTimers[heatLevel - 1];
		}
		else
		{
			maxBustDistance = roamMaxBustDistances[heatLevel - 1];
			bustTimer       = roamBustTimers[heatLevel - 1];
			evadeTimer      = roamEvadeTimers[heatLevel - 1];
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogDashed("[BAR] Updating PursuitBar");

			Globals::LogIndent("[BAR] maxBustDistance        :", maxBustDistance);
			Globals::LogIndent("[BAR] bustTimer              :", bustTimer);
			Globals::LogIndent("[BAR] evadeTimer             :", evadeTimer);
		}
    }
}