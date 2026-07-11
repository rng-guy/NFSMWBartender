#pragma once

#include <vector>
#include <optional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"
#include "ConfigParser.h"



namespace InteractiveMusic
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;
	
	// General	
	RELEASE_CONSTINIT std::vector<int> playlist;

	bool  transitionsEnabled = true;
	float lengthPerTrack     = 600.f; // seconds

	bool shuffleFirstTrack = true;
	bool shuffleAfterFirst = false;

	// Code caves
	size_t currentTrackID = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] int GetFirstTrack()
	{
		currentTrackID = (shuffleFirstTrack) ? Globals::prng.GenerateIndex(playlist.size()) : 0;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<1>("[MUS] First pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}



	[[nodiscard]] int GetNextTrack()
	{
		const size_t numTracks = playlist.size();

		currentTrackID += (shuffleAfterFirst and (numTracks > 2)) ? Globals::prng.GenerateNumber<size_t>(1, numTracks - 1) : 1;
		currentTrackID %= numTracks;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<1>("[MUS] Next pursuit theme:", playlist[currentTrackID] + 1);

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

			jmp dword ptr [nextTrackExit]
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

			jmp dword ptr [firstTrackExit]
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

			cmp byte ptr [transitionsEnabled], 1
			jne conclusion // transitions disabled

			fld dword ptr [esi + 0x48] // current track time
			fcomp dword ptr [lengthPerTrack]
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr [mainTransitionExit]
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

			cmp byte ptr [transitionsEnabled], 1
			jne conclusion // transitions disabled

			fld dword ptr [esi + 0x48] // current track time
			fcomp dword ptr [lengthPerTrack]
			fnstsw ax
			test ah, 0x41

			conclusion:
			jmp dword ptr [otherTransitionExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParsePlaylist(const HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("Playlist parsing:");

		const auto& sections     = parser.GetSections();
		const auto  foundSection = sections.find("Music:Playlist");

		if (foundSection == sections.end())
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no section provided");

			return false; // missing section; disable feature
		}

		// Validate and add track IDs
		const auto& pairs = foundSection->second;
		playlist.reserve(pairs.size());

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<3>(static_cast<int>(pairs.size()), "track(s) provided");

		constexpr auto ValuesToTrackID = [](const auto& values) -> std::optional<int>
		{
			if (values.size() != 1) return std::nullopt;

			const auto& name = values[0];

			if (name == "theme1") return 0;
			if (name == "theme2") return 1;
			if (name == "theme3") return 2;
			if (name == "theme4") return 3;

			return std::nullopt;
		};

		for (const auto& [key, values] : pairs)
		{
			if (key.find("track") == 0) // key starts with "track"
			{
				if (const auto trackID = ValuesToTrackID(values)) // value valid
					playlist.push_back(*trackID);

				else if constexpr (Globals::loggingEnabled) // value invalid
					Globals::logger.Log<3>('-', key, "(invalid value)");
			}
			else if constexpr (Globals::loggingEnabled) // key invalid
				Globals::logger.Log<3>('-', key, "(invalid format)");
		}

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<3>(static_cast<int>(playlist.size()), "track(s) valid");

		playlist.shrink_to_fit();
	
		return (not playlist.empty());
	}



	void ParseSettings(const HeatParameters::Parser& parser)
	{
		constexpr std::string_view section = "Playlist:Settings";

		transitionsEnabled = parser.ParseFromFile<float>(section, "lengthPerTrack", {lengthPerTrack});

		parser.ParseFromFile<bool>(section, "shuffleFirstTrack", {shuffleFirstTrack});
		parser.ParseFromFile<bool>(section, "shuffleAfterFirst", {shuffleAfterFirst});

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log<2>("Playlist:");

			for (size_t trackID = 0; trackID < playlist.size(); ++trackID)
				Globals::logger.Log<3>("track", static_cast<int>(trackID + 1), "= theme", playlist[trackID] + 1);

			if (transitionsEnabled)
				Globals::logger.Log<2>("Length per track:", lengthPerTrack);

			else
				Globals::logger.Log<2>("Transitions disabled");

			Globals::logger.Log<2>((shuffleFirstTrack) ? "Shuffled" : "Fixed", "first track");
			Globals::logger.Log<2>((shuffleAfterFirst) ? "Shuffled" : "Fixed", "follow-up track(s)");
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [MUS] InteractiveMusic");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Theme playlist
		if (not ParsePlaylist(parser)) return false; // no valid theme(s); disable feature

		ParseSettings(parser);

		// Code modifications 
		MemoryTools::MakeRangeJMP<nextTrackEntrance,       nextTrackExit>      (NextTrack);
		MemoryTools::MakeRangeJMP<firstTrackEntrance,      firstTrackExit>     (FirstTrack);
		MemoryTools::MakeRangeJMP<mainTransitionEntrance,  mainTransitionExit> (MainTransition);
		MemoryTools::MakeRangeJMP<otherTransitionEntrance, otherTransitionExit>(OtherTransition);

		// Status flag
		featureEnabled = true;

		return true;
	}
}