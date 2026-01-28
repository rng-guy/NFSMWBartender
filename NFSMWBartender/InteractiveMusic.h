#pragma once

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
	std::vector<int> playlist;

	bool  transitionsEnabled = true; 
	float lengthPerTrack     = 600.f; // seconds

	bool shuffleFirstTrack = true; 
	bool shuffleAfterFirst = false;

	// Code caves
	size_t currentTrackID = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	int GetFirstTrack()
	{
		currentTrackID = (shuffleFirstTrack) ? Globals::prng.GenerateIndex(playlist.size()) : 0;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] First pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}



	int GetNextTrack()
	{
		const size_t numTracks = playlist.size();

		currentTrackID += (shuffleAfterFirst and (numTracks > 2)) ? Globals::prng.GenerateNumber<size_t>(1, numTracks - 1) : 1;
		currentTrackID %= numTracks;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogIndent("[MUS] Next pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address nextTrackEntrance = 0x4E7A0D;
	constexpr address nextTrackExit     = 0x4E7A17;

	// Picks the next interactive pursuit track
	__declspec(naked) void NextTrack()
	{
		__asm
		{
			call GetNextTrack

			jmp dword ptr nextTrackExit
		}
	}



	constexpr address firstTrackEntrance = 0x4F8A5D;
	constexpr address firstTrackExit     = 0x4F8A6C;

	// Picks the first interactive pursuit track after a loading screen
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



	constexpr address mainTransitionEntrance = 0x71B1F4;
	constexpr address mainTransitionExit     = 0x71B205;

	// First function that checks for theme transitions
	__declspec(naked) void MainTransition()
	{
		__asm
		{
			mov dword ptr [esi + 0xC], eax

			cmp byte ptr transitionsEnabled, 0x1
			jne conclusion // transitions disabled

			fld dword ptr [esi + 0x48] // current track time
			fcomp dword ptr lengthPerTrack
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr mainTransitionExit
		}
	}



	constexpr address otherTransitionEntrance = 0x71B768;
	constexpr address otherTransitionExit     = 0x71B779;

	// Second function that checks for theme transitions
	__declspec(naked) void OtherTransition()
	{
		__asm
		{
			mov dword ptr [esi + 0xC], eax

			cmp byte ptr transitionsEnabled, 0x1
			jne conclusion // transitions disabled

			fld dword ptr [esi + 0x48] // current track time
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
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [MUS] InteractiveMusic");

		if (not parser.LoadFileWithLog(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Playlist
		const auto& sections     = parser.GetSections();
		const auto  foundSection = sections.find("Music:Playlist");

		if (foundSection != sections.end())
		{
			const std::string baseName    = "track";
			const auto&       definitions = foundSection->second;

			const HashContainers::Map<std::string, int> nameToIndex =
			{
				{"theme1", 0},
				{"theme2", 1},
				{"theme3", 2},
				{"theme4", 3}
			};

			if constexpr (Globals::loggingEnabled)
			{
				Globals::logger.LogLongIndent("Playlist parsing:");
				Globals::logger.LogLongIndent(' ', static_cast<int>(definitions.size()), "track(s) provided");
			}

			for (const auto& [definition, value] : definitions)
			{
				if (definition.find(baseName) == 0)
				{
					const auto foundName = nameToIndex.find(value);

					if (foundName != nameToIndex.end())
						playlist.push_back(foundName->second);

					else if constexpr (Globals::loggingEnabled)
						Globals::logger.LogLongIndent("  -", definition, "(invalid value)");
				}
				else if constexpr (Globals::loggingEnabled)
					Globals::logger.LogLongIndent("  -", definition, "(invalid format)");
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent(' ', static_cast<int>(playlist.size()), "track(s) valid");
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.LogLongIndent("  no track(s) provided");

		if (playlist.empty()) return false;

		// Playlist settings
		const std::string section = "Playlist:Settings";

		transitionsEnabled = parser.Parse<float>(section, "lengthPerTrack", lengthPerTrack);

		parser.Parse<bool>(section, "shuffleFirstTrack", shuffleFirstTrack);
		parser.Parse<bool>(section, "shuffleAfterFirst", shuffleAfterFirst);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.LogLongIndent("Playlist:");

			for (size_t trackID = 0; trackID < playlist.size(); ++trackID)
				Globals::logger.LogLongIndent("  track", static_cast<int>(trackID + 1), "= theme", playlist[trackID] + 1);

			if (transitionsEnabled)
				Globals::logger.LogLongIndent("Length per track:", lengthPerTrack);

			else
				Globals::logger.LogLongIndent("Transitions disabled");

			Globals::logger.LogLongIndent((shuffleFirstTrack) ? "Shuffled" : "Fixed", "first track");
			Globals::logger.LogLongIndent((shuffleAfterFirst) ? "Shuffled" : "Fixed", "follow-up track(s)");
		}

		// Code modifications 
		MemoryTools::MakeRangeJMP(NextTrack,       nextTrackEntrance,       nextTrackExit);
		MemoryTools::MakeRangeJMP(FirstTrack,      firstTrackEntrance,      firstTrackExit);
		MemoryTools::MakeRangeJMP(MainTransition,  mainTransitionEntrance,  mainTransitionExit);
		MemoryTools::MakeRangeJMP(OtherTransition, otherTransitionEntrance, otherTransitionExit);

		// Status flag
		featureEnabled = true;

		return true;
	}
}