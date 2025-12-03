#pragma once

#include <array>
#include <string>
#include <vector>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"
#include "ConfigParser.h"



namespace InteractiveMusic
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;
	
	// General	
	constexpr size_t maxNumTracks = 20;

	size_t numParsedTracks = 0;
	size_t numValidTracks  = 0;

	std::array<int, maxNumTracks> playlist = {};

	bool  transitionsEnabled = true;
	float lengthPerTrack     = 600.f; // seconds

	bool shuffleFirstTrack = true;
	bool shuffleAfterFirst = false;

	// Code caves
	size_t currentTrackID = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	int GetFirstTrack()
	{
		currentTrackID = (shuffleFirstTrack) ? Globals::prng.GenerateIndex(numValidTracks) : 0;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] First pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}



	int GetNextTrack()
	{
		currentTrackID += (shuffleAfterFirst and (numValidTracks > 2)) ? Globals::prng.GenerateNumber<size_t>(1, numValidTracks - 1) : 1;
		currentTrackID %= numValidTracks;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] Next pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address firstTrackEntrance = 0x4F8A5D;
	constexpr address firstTrackExit     = 0x4F8A6C;

	__declspec(naked) void FirstTrack()
	{
		__asm
		{
			call GetFirstTrack

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x2C]
			add esp, 0x14

			jmp dword ptr firstTrackExit
		}
	}



	constexpr address nextTrackEntrance = 0x4E7A0D;
	constexpr address nextTrackExit     = 0x4E7A17;

	__declspec(naked) void NextTrack()
	{
		__asm
		{
			call GetNextTrack

			jmp dword ptr nextTrackExit
		}
	}



	constexpr address mainTransitionEntrance = 0x71B1F4;
	constexpr address mainTransitionExit     = 0x71B205;

	__declspec(naked) void MainTransition()
	{
		__asm
		{
			mov dword ptr [esi + 0xC], eax

			cmp byte ptr transitionsEnabled, 0x1
			jne conclusion // transitions disabled

			fld dword ptr [esi + 0x48]
			fcomp dword ptr lengthPerTrack
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr mainTransitionExit
		}
	}



	constexpr address otherTransitionEntrance = 0x71B768;
	constexpr address otherTransitionExit     = 0x71B779;

	__declspec(naked) void OtherTransition()
	{
		__asm
		{
			mov dword ptr [esi + 0xC], eax

			cmp byte ptr transitionsEnabled, 0x1
			jne conclusion // transitions disabled

			fld dword ptr [esi + 0x48]
			fcomp dword ptr lengthPerTrack
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr otherTransitionExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		// Playlist
		std::array<std::string, maxNumTracks> rawPlaylist = {};

		numParsedTracks = parser.ParseFormatParameter<std::string, maxNumTracks>("Music:Playlist", "track{:02}", rawPlaylist);

		if (numParsedTracks == 0) return false;

		const HashContainers::Map<std::string, int> nameToIndex =
		{
			{"theme1", 0},
			{"theme2", 1},
			{"theme3", 2},
			{"theme4", 3}
		};

		for (size_t trackID = 0; trackID < maxNumTracks; ++trackID)
		{
			const auto foundName = nameToIndex.find(rawPlaylist[trackID]);

			if (foundName != nameToIndex.end())
				playlist[numValidTracks++] = foundName->second;
		}

		if (numValidTracks == 0) return false;

		// Settings
		const std::string section = "Playlist:Settings";

		transitionsEnabled = parser.ParseParameter<float>(section, "lengthPerTrack", lengthPerTrack);

		parser.ParseParameter<bool>(section, "shuffleFirstTrack", shuffleFirstTrack);
		parser.ParseParameter<bool>(section, "shuffleAfterFirst", shuffleAfterFirst);

		// Code modifications 
		MemoryTools::MakeRangeJMP(FirstTrack,      firstTrackEntrance,      firstTrackExit);
		MemoryTools::MakeRangeJMP(NextTrack,       nextTrackEntrance,       nextTrackExit);
		MemoryTools::MakeRangeJMP(MainTransition,  mainTransitionEntrance,  mainTransitionExit);
		MemoryTools::MakeRangeJMP(OtherTransition, otherTransitionEntrance, otherTransitionExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogSetupReport()
	{
		if (numParsedTracks == 0) return;

		Globals::logger.Log("  CONFIG [MUS] InteractiveMusic");

		if (numValidTracks != numParsedTracks)
		{
			if (numValidTracks == 0)
				Globals::logger.LogLongIndent("All", static_cast<int>(numParsedTracks), "tracks invalid");
				
			else 
				Globals::logger.LogLongIndent(static_cast<int>(numValidTracks), '/', static_cast<int>(numParsedTracks), "tracks valid");
		}
		else Globals::logger.LogLongIndent("All", static_cast<int>(numParsedTracks), "tracks valid");

		if (featureEnabled) 
		{
			for (size_t trackID = 0; trackID < numValidTracks; ++trackID)
				Globals::logger.LogLongIndent("  track", static_cast<int>(trackID + 1), "= theme", playlist[trackID] + 1);

			if (transitionsEnabled)
				Globals::logger.LogLongIndent("Length per track:", lengthPerTrack);

			else
				Globals::logger.LogLongIndent("Transitions disabled");

			Globals::logger.LogLongIndent((shuffleFirstTrack) ? "Shuffled" : "Fixed", "first track");
			Globals::logger.LogLongIndent((shuffleAfterFirst) ? "Shuffled" : "Fixed", "follow-up track(s)");
		}
		else Globals::logger.LogLongIndent("Playlist features disabled");
	}
}