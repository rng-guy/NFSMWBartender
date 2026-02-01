#pragma once

#include <array>
#include <vector>
#include <string>
#include <format>

#include "Globals.h"
#include "HeatParameters.h"



namespace RoadblockOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	constexpr size_t maxNumParts = 6;

	// Data structures
	enum RBPartType // C-style for compatibility
	{
		NONE     = 0,
		CAR      = 1,
		BLOCKADE = 2,
		SPIKES   = 3
	};

	struct RBPart
	{
		RBPartType partType = RBPartType::NONE;

		float offsetX = 0.f; // metres
		float offsetY = 0.f; // metres
		float angle   = 0.f; // revolutions
	};

	struct RBTable // matches vanilla layout
	{
		float  minRoadWidth    = 0.f; // metres
		size_t numCarsRequired = 0;

		RBPart parts[maxNumParts] = {}; // C-style for compatibility
	};

	struct RBSetup
	{
		std::string name;

		float maxRoadWidth = 0.f;
		bool  hasSpikes    = false;

		RBTable table = RBTable();

		HeatParameters::Pair<int> chances{100, {0}};
	};

	// Heat parameters
	HeatParameters::Pair<int> spawnCalloutChances(100, {0, 100});
	HeatParameters::Pair<int> spikeCalloutChances(50,  {0, 100});
	
	// Code caves
	std::vector<RBSetup> roadblockSetups;

	bool hasSpikes = false;
	int  spikeLane = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall RequestCallout(const address pursuit)
	{
		if (Globals::IsInCooldownMode(pursuit))    return;
		if (not Globals::IsPlayerPursuit(pursuit)) return;

		if (Globals::prng.DoTrial<int>(spawnCalloutChances.current))
		{
			if (hasSpikes and Globals::prng.DoTrial<int>(spikeCalloutChances.current))
			{
				const auto CallOutSpikes = reinterpret_cast<void (__cdecl*)(int)>(0x71DAC0);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Spikes callout");

				CallOutSpikes(spikeLane);
			}
			else
			{
				const auto CallOutRegular = reinterpret_cast<void (*)()>(0x71DAA0);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("Regular callout");

				CallOutRegular();
			}
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("No callout");
	}





	// Replacement functions ------------------------------------------------------------------------------------------------------------------------

	const RBTable* __cdecl SelectRoadblockTable
	(
		const float  roadWidth, 
		const size_t maxNumCars, 
		const bool   needsSpikes
	) {
		static std::vector<const RBSetup*> candidates;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log<1>("[RBL] Roadblock request", (needsSpikes) ? "(spikes)" : "(regular)");
			Globals::logger.Log<2>("Max. cars:", static_cast<int>(maxNumCars));
			Globals::logger.Log<2>("Road width:", roadWidth);
		}

		// Find all eligible setups, and the best-fitting one as backup
		int totalChance = 0;

		const RBSetup* bestFit = nullptr;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (setup.chances.current < 1)                continue;
			if (setup.hasSpikes != needsSpikes)           continue;
			if (setup.table.minRoadWidth > roadWidth)     continue;
			if (setup.table.numCarsRequired > maxNumCars) continue;

			if (setup.maxRoadWidth > roadWidth)
			{
				totalChance += setup.chances.current;
				candidates.push_back(&setup);
			}
			
			if ((not bestFit) or (setup.table.minRoadWidth > bestFit->table.minRoadWidth))
				bestFit = &setup;
		}

		// Attempt to select an eligible setup at random
		if (not candidates.empty())
		{
			int       cumulativeChance = 0;
			const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, totalChance);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(static_cast<int>(candidates.size()), "candidate(s)");

			for (const RBSetup* const setup : candidates)
			{
				cumulativeChance += setup->chances.current;

				if (cumulativeChance >= chanceThreshold)
				{
					candidates.clear();

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<2>("Setup:", setup->name);

					return &(setup->table);
				}
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [RBL] Failed to select roadblock setup");

			candidates.clear();
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("No candidate(s)");

		// Fall back to best-fitting setup (if available)
		if (bestFit)
		{
			if constexpr (Globals::loggingEnabled)
			{
				Globals::logger.Log<2>("Best width:", bestFit->table.minRoadWidth);
				Globals::logger.Log<2>("Setup:",      bestFit->name);
			}

			return &(bestFit->table);
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("No best fit");

		return nullptr;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address spikesLaneEntrance = 0x43E574;
	constexpr address spikesLaneExit     = 0x43E57B;

	// Records the last spike-strip position
	__declspec(naked) void SpikesLane()
	{
		__asm
		{
			mov byte ptr hasSpikes, 0x1
			mov dword ptr spikeLane, eax

			// Execute original code and resume
			lea ecx, dword ptr [esp + 0xA4]

			jmp dword ptr spikesLaneExit
		}
	}



	constexpr address radioRequestEntrance = 0x43E20C;
	constexpr address radioRequestExit     = 0x43E213;

	// Requests a callout over the radio after a roadblock spawn
	__declspec(naked) void RadioRequest()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			mov ecx, dword ptr [esp + 0x4C4]
			call RequestCallout // ecx: pursuit

			mov al, 0x1

			conclusion:
			// Execute original code and resume
			lea ecx, dword ptr [esp + 0x4B4]

			jmp dword ptr radioRequestExit
		}
	}



	constexpr address spawnFailureEntrance = 0x43E1DA;
	constexpr address spawnFailureExit     = 0x43E1E0;

	// Prevents failed roadblock requests from stalling cop spawns
	__declspec(naked) void SpawnFailure()
	{
		__asm
		{
			// Execute original code first
			test ecx, ecx
			mov dword ptr [esp + 0x18], ecx
			jne conclusion // found suitable setup

			cmp dword ptr [esp + 0x4C0], 0x43E7D6
			jne regular // not HeavyStrategy 4

			mov ecx, dword ptr [esp + 0x4C4] // pursuit
			call Globals::ClearSupportRequest
			jmp restore                      // was HeavyStrategy 4

			regular:
			cmp dword ptr [esp + 0x4C0], 0x43EC3A
			jne restore // not non-Strategy roadblock

			mov eax, dword ptr [esp + 0x4C4] // pursuit
			mov edx, dword ptr [esp + 0x54]  // AICopManager

			mov byte ptr [eax + 0x190], cl  // request status
			mov dword ptr [edx + 0xBC], ecx // roadblock pursuit
			mov dword ptr [edx + 0xB8], ecx // max. car count

			restore:
			xor ecx, ecx
			
			conclusion:
			mov byte ptr hasSpikes, 0x0

			jmp dword ptr spawnFailureExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Fixes cop-spawn stalling due to failed roadblock spawn attempts
		MemoryTools::MakeRangeJMP(SpawnFailure, spawnFailureEntrance, spawnFailureExit);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RBL] RoadblockOverrides");

		if (not parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Roadblocks:Radio", spawnCalloutChances, spikeCalloutChances);

		// Roadblock setups
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("Roadblock setups:");

		const auto&       sections = parser.GetSections();
		const std::string baseName = "Setups:";

		size_t maxNumSetups = 0;

		for (const auto& [section, contents] : sections)
			if (section.find(baseName) == 0) ++maxNumSetups;

		// Check if there are any setups
		if (maxNumSetups == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no setup(s) provided");

			return false;
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<3>(static_cast<int>(maxNumSetups), "setup(s) provided");

		roadblockSetups.reserve(maxNumSetups);

		std::array<bool, maxNumParts> isValids = {};

		std::array<int,   maxNumParts> partTypes    = {};
		std::array<float, maxNumParts> partOffsetsX = {};
		std::array<float, maxNumParts> partOffsetsY = {};
		std::array<float, maxNumParts> orientations = {};

		const std::string partFormat = "part{:02}";

		size_t numRegularRoadblocks = 0;
		size_t numSpikeRoadblocks   = 0;

		std::string name;

		// Attempt to parse each setup
		for (const auto& [section, contents] : sections)
		{
			if (section.find(baseName) > 0) continue;

			float minRoadWidth = 0.f;
			float maxRoadWidth = 0.f;

			name = section.substr(baseName.length());

			// Check if "extent" is provided
			if (not parser.ParseFromFile<float, float>(section, "extent", {minRoadWidth, {0.f}}, {maxRoadWidth, {0.f}}))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', name, "(missing extent)");

				continue;
			}

			// Check if "extent" is valid
			if (maxRoadWidth <= minRoadWidth)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', name, "(invalid extent)");

				continue;
			}

			// Parse roadblock parts
			RBSetup& setup = roadblockSetups.emplace_back();

			setup.table.minRoadWidth = minRoadWidth;
			setup.maxRoadWidth       = maxRoadWidth;

			isValids = parser.ParseFormat<maxNumParts, int, float, float, float>
			(
				section,
				partFormat,
				{partTypes},
				{partOffsetsX},
				{partOffsetsY},
				{orientations}
			);

			// Validate roadblock parts
			size_t numValidParts = 0;

			for (size_t partID = 0; partID < maxNumParts; ++partID)
			{
				if (not isValids[partID]) continue;

				switch (partTypes[partID])
				{
				case RBPartType::CAR:
					++(setup.table.numCarsRequired);
					break;

				case RBPartType::BLOCKADE:
					break;

				case RBPartType::SPIKES:
					setup.hasSpikes = true;
					break;

				default:
					continue;
				}

				setup.table.parts[numValidParts++] =
				{
					static_cast<RBPartType>(partTypes[partID]),
					partOffsetsX[partID],
					partOffsetsY[partID],
					orientations[partID]
				};
			}
			
			// Add or discard setup depending on validity
			if (setup.table.numCarsRequired > 0)
			{
				setup.name = name;

				if constexpr (Globals::loggingEnabled)
				{
					if (setup.hasSpikes)
						++numSpikeRoadblocks;

					else
						++numRegularRoadblocks;
				}

				HeatParameters::Parse(parser, section, setup.chances);
			}
			else
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', name, "(no car(s))");

				roadblockSetups.pop_back();
			}
		}

		if (roadblockSetups.empty())
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no setup(s) valid");

			return false;
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log<3>(static_cast<int>(roadblockSetups.size()), "setup(s) valid");
			Globals::logger.Log<3>(static_cast<int>(numRegularRoadblocks),   "regular");
			Globals::logger.Log<3>(static_cast<int>(numSpikeRoadblocks),     "spikes");
		}

		// Code Changes
		MemoryTools::MakeRangeNOP(0x71F184, 0x71F19F); // regular callout
		MemoryTools::MakeRangeNOP(0x71F091, 0x71F096); // spikes  callout

		MemoryTools::MakeRangeJMP(SelectRoadblockTable, 0x4063D0, 0x40644A);

		MemoryTools::MakeRangeJMP(SpikesLane,   spikesLaneEntrance,   spikesLaneExit);
		MemoryTools::MakeRangeJMP(RadioRequest, radioRequestEntrance, radioRequestExit);

		ApplyFixes();

		// Status flag
		featureEnabled = true;

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		// Heat parameters
		spawnCalloutChances.SetToHeat(isRacing, heatLevel);
		spikeCalloutChances.SetToHeat(isRacing, heatLevel);

		// Roadblock setups
		size_t numRegularRoadblocks = 0;
		size_t numSpikeRoadblocks   = 0;

		for (RBSetup& setup : roadblockSetups)
		{
			setup.chances.SetToHeat(isRacing, heatLevel);

			if (setup.chances.current > 0)
			{
				if (setup.hasSpikes)
					++numSpikeRoadblocks;

				else 
					++numRegularRoadblocks;
			}
		}

		// With logging disabled, the compiler optimises the unsigned integers away
		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [RBL] RoadblockOverrides");

			spawnCalloutChances.Log("spawnCalloutChance      ");
			spikeCalloutChances.Log("spikeCalloutChance      ");

			Globals::logger.Log<2>("numRegularRoadblocks:   ", static_cast<int>(numRegularRoadblocks));
			Globals::logger.Log<2>("numSpikeRoadblocks:     ", static_cast<int>(numSpikeRoadblocks));
		}
	}
}