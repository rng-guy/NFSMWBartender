#pragma once

#include <array>
#include <string>
#include <vector>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"
#include "ConfigParser.h"



namespace InteractiveMusic
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// General
	constexpr size_t maxNumTracks = 20;

	std::vector<int> tracks;

	bool  transitionsEnabled = true;
	float lengthPerTrack     = 600.f; // seconds

	bool shuffleFirstTrack = true;
	bool shuffleAfterFirst = false;

	// Code caves
	size_t currentTrackID = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	int GetFirstTrackType()
	{
		currentTrackID = (shuffleFirstTrack) ? Globals::prng.GenerateIndex(tracks.size()) : 0;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] First track type:", tracks[currentTrackID]);

		return tracks[currentTrackID];
	}



	int GetNextTrackType()
	{
		currentTrackID += (shuffleAfterFirst and (tracks.size() > 2)) ? Globals::prng.GenerateNumber<size_t>(1, tracks.size() - 1) : 1;
		currentTrackID %= tracks.size();

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] Next track type:", tracks[currentTrackID]);

		return tracks[currentTrackID];
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address firstTrackEntrance = 0x4F8A5D;
	constexpr address firstTrackExit     = 0x4F8A6C;

	__declspec(naked) void FirstTrack()
	{
		__asm
		{
			call GetFirstTrackType

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
			call GetNextTrackType

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
			jne conclusion // track-changing disabled

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
			jne conclusion // track-changing disabled

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

		// General parameters
		std::array<int, maxNumTracks> allTracks = {};

		const auto isValids = parser.ParseParameterTable<int>
		(
			"Music:Tracks",
			"track{:02}",
			ConfigParser::FormatParameter<int, maxNumTracks>(allTracks)
		);

		tracks.reserve(maxNumTracks);

		for (size_t trackID = 0; trackID < maxNumTracks; ++trackID)
		{
			if (isValids[trackID])
				tracks.push_back(allTracks[trackID]);
		}

		if (tracks.empty()) return false;

		const std::string section = "Music:Settings";

		transitionsEnabled = parser.ParseParameter<float>(section, "lengthPerTrack", lengthPerTrack);

		parser.ParseParameter<bool>(section, "shuffleFirstTrack", shuffleFirstTrack);
		parser.ParseParameter<bool>(section, "shuffleAfterFirst", shuffleAfterFirst);

		// Code caves
		MemoryTools::DigCodeCave(FirstTrack,      firstTrackEntrance,      firstTrackExit);
		MemoryTools::DigCodeCave(NextTrack,       nextTrackEntrance,       nextTrackExit);
		MemoryTools::DigCodeCave(MainTransition,  mainTransitionEntrance,  mainTransitionExit);
		MemoryTools::DigCodeCave(OtherTransition, otherTransitionEntrance, otherTransitionExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [MUS] InteractiveMusic");

		auto   iterator   = tracks.begin();
		size_t numRemoved = 0;

		while (iterator != tracks.end())
		{
			if ((*iterator < 0) or (*iterator > 3))
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (numRemoved == 0)
						Globals::logger.LogLongIndent("Tracks vector");

					Globals::logger.LogLongIndent("  -", *iterator);
				}

				iterator = tracks.erase(iterator);

				++numRemoved;
			}
			else ++iterator;
		}

		if constexpr (Globals::loggingEnabled)
		{
			if (numRemoved > 0)
			{
				if (tracks.empty())
					Globals::logger.LogLongIndent("  using vanilla tracks");

				else
					Globals::logger.LogLongIndent("  tracks left:", (int)tracks.size());
			}
			else Globals::logger.LogLongIndent("Tracks:", (int)tracks.size());

			if (transitionsEnabled)
				Globals::logger.LogLongIndent("Length per track:", lengthPerTrack);

			else
				Globals::logger.LogLongIndent("Transitions disabled");

			Globals::logger.LogLongIndent((shuffleFirstTrack) ? "Shuffled" : "Fixed", "first track");
			Globals::logger.LogLongIndent((shuffleAfterFirst) ? "Shuffled" : "Fixed", "follow-up track(s)");
		}

		if (tracks.empty())
			tracks = {0, 1, 2, 3};
	}
}