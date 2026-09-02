#pragma once

#include <vector>
#include <optional>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace InteractiveMusic
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[MUS]";
	constexpr Globals::LogLiteral logName = "InteractiveMusic";
	
	// General	
	RELEASE_CONSTINIT std::vector<int> playlist;

	bool  transitionsEnabled = true;
	float lengthPerTrack     = 600.f; // seconds

	bool shuffleFirstTrack = true;
	bool shuffleAfterFirst = false;

	// Assembly detours
	size_t currentTrackID = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] int GetFirstTrack()
	{
		currentTrackID = (shuffleFirstTrack) ? Globals::pRNG.GenerateIndex(playlist) : 0;

		if constexpr (Globals::loggingEnabled)
			Globals::LogTagged(logTag, "First pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}



	[[nodiscard]] int GetNextTrack()
	{
		const size_t numTracks = playlist.size();

		if (shuffleAfterFirst and (numTracks > 2))
			currentTrackID += Globals::pRNG.GenerateNumber<size_t>(1, numTracks - 1);

		else ++currentTrackID;

		currentTrackID %= numTracks;

		if constexpr (Globals::loggingEnabled)
			Globals::LogTagged(logTag, "Next pursuit theme:", playlist[currentTrackID] + 1);

		return playlist[currentTrackID];
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Picks the next interactive pursuit track to play in an active pursuit
	ASSEMBLY_DETOUR(NextTrack, /* begin = */ 0x4E7A0D, /* end = */ 0x4E7A17)
	{
		__asm
		{
			call GetNextTrack

			EXIT_ASSEMBLY_DETOUR(NextTrack)
		}
	}



	// Picks the first interactive pursuit track to play
	ASSEMBLY_DETOUR(FirstTrack, 0x4F8A5D, 0x4F8A6C)
	{
		__asm
		{
			call GetFirstTrack

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x2C]
			add esp, 0x14

			EXIT_ASSEMBLY_DETOUR(FirstTrack)
		}
	}



	// First function that checks for theme transitions
	ASSEMBLY_DETOUR(MainTransition, 0x71B1F4, 0x71B205)
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
			EXIT_ASSEMBLY_DETOUR(MainTransition)
		}
	}



	// Second function that checks for theme transitions
	ASSEMBLY_DETOUR(OtherTransition, 0x71B768, 0x71B779)
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
			EXIT_ASSEMBLY_DETOUR(OtherTransition)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool ExtractPlaylist(const ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Playlist parsing:");

		const auto* const section = parser.GetSection("Music:Playlist");

		if (not section)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail("no track(s) provided");

			return false; // missing section; disable feature
		}

		// Validate and add track IDs
		playlist.reserve(section->size());

		if constexpr (Globals::loggingEnabled)
			Globals::LogDetail(Globals::LogDec(section->size()), "track(s) provided");

		constexpr auto ValuesToThemeID = [](const auto& values) -> std::optional<int>
		{
			if (values.size() != 1) return std::nullopt; // value-count mismatch

			const auto& name = values[0];

			if (name == "theme1") return 0;
			if (name == "theme2") return 1;
			if (name == "theme3") return 2;
			if (name == "theme4") return 3;

			return std::nullopt;
		};

		for (const auto& [key, values] : *section)
		{
			if (not key.starts_with("track"))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogDetail('-', key, "(invalid format)");

				continue; // invalid key
			}

			if (const auto themeID = ValuesToThemeID(values))
				playlist.push_back(*themeID);

			else if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', key, "(invalid value)");
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogDetail(Globals::LogDec(playlist.size()), "track(s) valid");

			if (not playlist.empty())
			{
				Globals::LogPlain("Playlist:");

				for (size_t trackID = 0; trackID < playlist.size(); ++trackID)
					Globals::LogDetail("track", Globals::LogDec(trackID + 1), "= theme", playlist[trackID] + 1);
			}
		}

		playlist.shrink_to_fit();
	
		return (not playlist.empty());
	}



	void ExtractSettings(const ConfigParser::Parser& parser)
	{
		const auto* const section = parser.GetSection("Playlist:Settings");
		
		transitionsEnabled = parser.ExtractScalars<float>(section, "lengthPerTrack", {lengthPerTrack, {10.f}});

		parser.ExtractScalars<bool>(section, "shuffleFirstTrack", {shuffleFirstTrack});
		parser.ExtractScalars<bool>(section, "shuffleAfterFirst", {shuffleAfterFirst});

		if constexpr (Globals::loggingEnabled)
		{
			if (transitionsEnabled)
				Globals::LogPlain("Length per track:", lengthPerTrack);

			else Globals::LogPlain("Transitions disabled");

			Globals::LogPlain((shuffleFirstTrack) ? "Shuffled" : "Fixed", "first track");
			Globals::LogPlain((shuffleAfterFirst) ? "Shuffled" : "Fixed", "follow-up track(s)");
		}
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileCosmetic)) return false;

		// Theme playlist
		if (not ExtractPlaylist(parser)) return false; // no valid theme(s); disable feature

		ExtractSettings(parser);

		// Code modifications 
		PATCH_ASSEMBLY_DETOUR(NextTrack);
		PATCH_ASSEMBLY_DETOUR(FirstTrack);
		PATCH_ASSEMBLY_DETOUR(MainTransition);
		PATCH_ASSEMBLY_DETOUR(OtherTransition);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}