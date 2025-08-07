#pragma once

#include <Windows.h>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace PursuitBar 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	Globals::HeatParametersPair<float> maxBustDistances(15.f); // metres
	Globals::HeatParametersPair<float> evadeTimers     (7.f);  // seconds
	Globals::HeatParametersPair<float> bustTimers      (5.f);

	// Conversions
	float halfEvadeRate = .5f / evadeTimers.current; // Hertz
	float bustRate      = 1.f / bustTimers.current;





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

			fld dword ptr maxBustDistances.current
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
		Globals::ParseHeatParameters<float, float>(parser, "Busting:General", {bustTimers,  .001f}, {maxBustDistances, 0.f});
		Globals::ParseHeatParameters<float>       (parser, "Evading:Timer",   {evadeTimers, .001f});

		// Code caves
		MemoryEditor::Write<float*>(&bustRate,              0x40AEDB);
		MemoryEditor::Write<float*>(&halfEvadeRate,         0x444A3A);
		MemoryEditor::Write<float*>(&(bustTimers.current),  0x4445CE);
        MemoryEditor::Write<float*>(&(evadeTimers.current), 0x4448E6, 0x444802, 0x4338F8);

        MemoryEditor::DigCodeCave(MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);

		return (featureEnabled = true);
    }



    void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
        if (not featureEnabled) return;

		bustTimers.SetToHeat      (isRacing, heatLevel);
		maxBustDistances.SetToHeat(isRacing, heatLevel);
		evadeTimers.SetToHeat     (isRacing, heatLevel);

		halfEvadeRate = .5f / evadeTimers.current;
		bustRate      = 1.f / bustTimers.current;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[BAR] PursuitBar");

			Globals::LogLongIndent("bustTimer              :", bustTimers.current);
			Globals::LogLongIndent("maxBustDistance        :", maxBustDistances.current);
			Globals::LogLongIndent("evadeTimer             :", evadeTimers.current);
		}
    }
}