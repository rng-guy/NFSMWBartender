#pragma once

#include <Windows.h>

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
	
	// Heat levels
	Globals::HeatParametersPair<float> maxBustDistances;
	Globals::HeatParametersPair<float> evadeTimers;
	Globals::HeatParametersPair<float> bustTimers;

	// Conversions
	float halfEvadeRate = .5f / evadeTimer;
	float bustRate      = 1.f / bustTimer;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address maxBustDistanceEntrance = 0x444483;
	constexpr address maxBustDistanceExit     = 0x44448B;

	__declspec(naked) void MaxBustDistance()
	{
		static constexpr float evadeStateThreshold    = 0.f;
		static constexpr float obstructedBustDistance = 0.f;

		__asm
		{
			fld dword ptr [esi + 0x168] // EVADE progress
			fcomp dword ptr evadeStateThreshold
			fnstsw ax
			test ah, ah
			je obstructed               // in EVADE state

			fld dword ptr maxBustDistance
			jmp conclusion

			obstructed:
			fld dword ptr obstructedBustDistance

			conclusion:
			// Execute original code and resume
			test bl, bl

			jmp dword ptr maxBustDistanceExit
		}
	}





    // State management -----------------------------------------------------------------------------------------------------------------------------

    bool Initialise(ConfigParser::Parser& parser)
    {
		if (not parser.LoadFile(Globals::configPathBasic + "Others.ini")) return false;

		// Pursuit parameters
		Globals::ParseHeatParameters<float, float>
		(
			parser,
			"Busting:General",
			{bustTimers,       bustTimer,       .001f},
			{maxBustDistances, maxBustDistance, 0.f}
		);

		Globals::ParseHeatParameters<float>
		(
			parser,
			"Evading:Timer",
			{evadeTimers, evadeTimer, .001f}
		);

		// Code caves
        MemoryEditor::Write<float*>(&bustTimer,     0x4445CE);
		MemoryEditor::Write<float*>(&bustRate,      0x40AEDB);
        MemoryEditor::Write<float*>(&evadeTimer,    0x4448E6, 0x444802, 0x4338F8);
		MemoryEditor::Write<float*>(&halfEvadeRate, 0x444A3A);

        MemoryEditor::DigCodeCave(MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);

		return (featureEnabled = true);
    }



    void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		bustTimer       = bustTimers      (isRacing, heatLevel);
		maxBustDistance = maxBustDistances(isRacing, heatLevel);
		evadeTimer      = evadeTimers     (isRacing, heatLevel);

		halfEvadeRate = .5f / evadeTimer;
		bustRate      = 1.f / bustTimer;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[BAR] PursuitBar");

			Globals::LogLongIndent("bustTimer              :", bustTimer);
			Globals::LogLongIndent("maxBustDistance        :", maxBustDistance);
			Globals::LogLongIndent("evadeTimer             :", evadeTimer);
		}
    }
}