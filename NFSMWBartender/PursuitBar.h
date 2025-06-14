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
	float evadeTimer      = 7.f; // seconds
	float bustTimer       = 5.f; // seconds
	
	// General Heat levels
	std::array<float, Globals::maxHeatLevel> maxBustDistances = {};
	std::array<float, Globals::maxHeatLevel> evadeTimers      = {};
	std::array<float, Globals::maxHeatLevel> bustTimers       = {};

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
		if (not parser.LoadFile(Globals::configBasicPath + "General.ini")) return;

		parser.ParseParameterTable
		(
			"Busting:General",
			Globals::configFormat,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxBustDistances, maxBustDistance, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(bustTimers,       bustTimer,       .001f)
		);

		parser.ParseFormatParameter<float>("Evading:Timer", Globals::configFormat, evadeTimers, evadeTimer, .001f);

        MemoryEditor::Write<WORD>  (0x35D8,      0x40AED9); // opcode fdiv m
        MemoryEditor::Write<float*>(&bustTimer,  0x4445CE,  0x40AEDB);
        MemoryEditor::Write<float*>(&evadeTimer, 0x4448E6,  0x444802,  0x4338F8);

        MemoryEditor::DigCodeCave(&MaxBustDistance, maxBustDistanceEntrance, maxBustDistanceExit);
        MemoryEditor::DigCodeCave(&EvadeBar,        evadeBarEntrance,        evadeBarExit);

        featureEnabled = true;
    }



    void SetToHeat(const size_t heatLevel)
    {
        if (not featureEnabled) return;

        maxBustDistance = maxBustDistances[heatLevel - 1];
        bustTimer       = bustTimers[heatLevel - 1];
		evadeTimer      = evadeTimers[heatLevel - 1];
    }
}