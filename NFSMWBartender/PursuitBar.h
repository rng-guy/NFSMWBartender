#pragma once

#include "Globals.h"
#include "MemoryEditor.h"
#include "HeatParameters.h"



namespace PursuitBar 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<float> maxBustDistances(15.f); // metres
	HeatParameters::Pair<float> evadeTimers     (7.f);  // seconds
	HeatParameters::Pair<float> bustTimers      (5.f);

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

    bool Initialise(HeatParameters::Parser& parser)
    {
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Others.ini")) return false;

		// Pursuit parameters
		HeatParameters::Parse<float, float>(parser, "Busting:General", {bustTimers,  .001f}, {maxBustDistances, 0.f});
		HeatParameters::Parse<float>       (parser, "Evading:Timer",   {evadeTimers, .001f});

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
			Globals::logger.Log("    HEAT [BAR] PursuitBar");

			Globals::logger.LogLongIndent("bustTimer              :", bustTimers.current);
			Globals::logger.LogLongIndent("maxBustDistance        :", maxBustDistances.current);
			Globals::logger.LogLongIndent("evadeTimer             :", evadeTimers.current);
		}
    }
}