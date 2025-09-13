#pragma once

#include <array>
#include <vector>

#include "Globals.h"
#include "MemoryEditor.h"
#include "HeatParameters.h"
#include "ConfigParser.h"



namespace InteractiveMusic
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	HeatParameters::Pair<bool>  transitionEnableds(false);
	HeatParameters::Pair<float> transitionTimers  (600.f); // seconds

	// Code caves
	constexpr size_t maxNumTracks = 20;

	std::vector<int> tracks;

	size_t currentTrackID    = 0;
	bool   shuffleFirstTrack = true;
	bool   shuffleAfterFirst = false;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	int GetFirstTrack()
	{
		if (shuffleFirstTrack)
		{
			switch (tracks.size())
			{
			case 0:
				currentTrackID = Globals::prng.Generate<size_t>(0, 4);
				break;

			case 1:
				currentTrackID = 0;
				break;

			default:
				currentTrackID = Globals::prng.Generate<size_t>(0, tracks.size());
			}
		}
		else currentTrackID = 0;

		const int firstTrack = (tracks.empty()) ? (int)currentTrackID : tracks[currentTrackID];

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] First track:", firstTrack, (int)currentTrackID);

		return firstTrack;
	}



	int GetNextTrack()
	{
		if (shuffleAfterFirst)
		{
			switch (tracks.size())
			{
			case 0:
				currentTrackID += Globals::prng.Generate<size_t>(1, 3);
				break;

			case 1:
				currentTrackID = 0;
				break;

			case 2:
				currentTrackID++;
				break;

			default:
				currentTrackID += Globals::prng.Generate<size_t>(1, tracks.size() - 1);
			}
		}
		else currentTrackID++;

		const int nextTrack = (tracks.empty()) ? (int)(currentTrackID %= 4) : tracks[currentTrackID %= tracks.size()];

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] Next track:", nextTrack, (int)currentTrackID);

		return nextTrack;
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

			cmp dword ptr transitionEnableds.current, 0x1
			jne conclusion // track-changing disabled

			fld dword ptr [esi + 0x48]
			fcomp dword ptr transitionTimers.current
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

			cmp dword ptr transitionEnableds.current, 0x1
			jne conclusion // track-changing disabled

			fld dword ptr [esi + 0x48]
			fcomp dword ptr transitionTimers.current
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

		for (size_t trackID = 0; trackID < maxNumTracks; trackID++)
		{
			if (isValids[trackID])
				tracks.push_back(allTracks[trackID]);
		}

		parser.ParseParameter<bool>("Music:Tracks", "shuffleFirstTrack", shuffleFirstTrack);
		parser.ParseParameter<bool>("Music:Tracks", "shuffleAfterFirst", shuffleAfterFirst);

		// Heat parameters
		HeatParameters::ParseOptional<float>(parser, "Music:Transition", transitionEnableds, {transitionTimers, 1.f});

		// Code caves
		MemoryEditor::DigCodeCave(FirstTrack,      firstTrackEntrance,      firstTrackExit);
		MemoryEditor::DigCodeCave(NextTrack,       nextTrackEntrance,       nextTrackExit);
		MemoryEditor::DigCodeCave(MainTransition,  mainTransitionEntrance,  mainTransitionExit);
		MemoryEditor::DigCodeCave(OtherTransition, otherTransitionEntrance, otherTransitionExit);

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

				numRemoved++;
			}
			else iterator++;
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.LogLongIndent((numRemoved > 0) ? "  tracks left:" : "Tracks:", (int)tracks.size());
			Globals::logger.LogLongIndent((shuffleFirstTrack) ? "Shuffled" : "Fixed", "first track");
			Globals::logger.LogLongIndent((shuffleAfterFirst) ? "Shuffled" : "Fixed", "next track(s)");
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		transitionEnableds.SetToHeat(isRacing, heatLevel);
		transitionTimers  .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			if (transitionEnableds.current)
			{
				Globals::logger.Log("    HEAT [MUS] InteractiveMusic");
				Globals::logger.LogLongIndent("transitionTimer         ", transitionTimers.current);
			}
		}
	}
}