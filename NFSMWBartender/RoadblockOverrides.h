#pragma once

#include <array>
#include <vector>
#include <string>
#include <format>

#include "Globals.h"
#include "HeatParameters.h"



namespace RoadblockOverrides
{

	// Roadblock (part) structs ---------------------------------------------------------------------------------------------------------------------

	// Parts (matches vanilla layout)
	enum RBPartType
	{
		NONE     = 0,
		CAR      = 1,
		BLOCKADE = 2,
		SPIKES   = 3
	};

	struct RBPart
	{
		RBPartType type = RBPartType::NONE;

		float offsetX     = 0.f; // metres
		float offsetY     = 0.f; // metres
		float orientation = 0.f; // full rotations
	};

	static_assert(sizeof(RBPart) == 16, "Part size mismatch");



	// Tables (matches vanilla layout)
	constexpr size_t maxNumParts = 6;

	struct RBTable
	{
		float  minRoadWidth    = 0.f; // metres
		size_t numCarsRequired = 0;   // cars

		RBPart parts[maxNumParts];
	};

	static_assert(sizeof(RBTable) == 104, "Table size mismatch");



	// Setups (mod-specific)
	struct RBSetup
	{
		std::string name;

		RBTable standard;
		RBTable mirrored;

		bool hasSpikes = false;

		float maxRoadWidth = 0.f; // metres
		float mirrorChance = 0.f; // percent

		HeatParameters::Pair<int> chances{100, {0}}; // percent


		const RBTable* GetRandomTable() const
		{
			const bool isMirrored = Globals::prng.DoTrial<float>(this->mirrorChance);

			if constexpr (Globals::loggingEnabled)
			{
				if (isMirrored)
					Globals::logger.Log<2>("Setup:", this->name, "(mirrored)");

				else
					Globals::logger.Log<2>("Setup:", this->name);
			}

			return &((isMirrored) ? this->mirrored : this->standard);
		}


		bool MirrorEnabled() const
		{
			return (this->mirrorChance > 0.f);
		}
	};



	// Counter for logging
	struct SetupCounter
	{
		size_t numRegular = 0;
		size_t numSpike   = 0;

		size_t numMirrorRegular = 0;
		size_t numMirrorSpikes  = 0;


		void ResetCounts()
		{
			this->numRegular = 0;
			this->numSpike   = 0;

			this->numMirrorRegular = 0;
			this->numMirrorSpikes  = 0;
		}


		void CountSetup(const RBSetup& setup)
		{
			if (setup.hasSpikes)
			{
				++numSpike;

				if (setup.MirrorEnabled())
					++numMirrorSpikes;
			}
			else
			{
				++numRegular;

				if (setup.MirrorEnabled())
					++numMirrorRegular;
			}
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	HeatParameters::Pair<float> spawnCalloutChances(100.f, {0.f, 100.f});
	HeatParameters::Pair<float> spikeCalloutChances(50.f,  {0.f, 100.f});
	
	// Code caves
	std::vector<RBSetup> roadblockSetups;

	bool hasSpikes = false;
	int  spikeLane = 0;

	// Logging
	SetupCounter counter;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall RequestCallout(const address pursuit)
	{
		if (Globals::IsInCooldownMode(pursuit))    return;
		if (not Globals::IsPlayerPursuit(pursuit)) return;

		if (Globals::prng.DoTrial<float>(spawnCalloutChances.current))
		{
			if (hasSpikes and Globals::prng.DoTrial<float>(spikeCalloutChances.current))
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

		// Use both the modded and vanilla methods to find eligible setups
		int totalChance = 0;

		const RBSetup* vanillaResult = nullptr;

		for (const RBSetup& setup : roadblockSetups)
		{
			// Standard / mirrored setups share constraints
			if (setup.chances.current < 1)                   continue;
			if (setup.hasSpikes != needsSpikes)              continue;
			if (setup.standard.minRoadWidth > roadWidth)     continue;
			if (setup.standard.numCarsRequired > maxNumCars) continue;

			if (setup.maxRoadWidth > roadWidth)
			{
				totalChance += setup.chances.current;
				candidates.push_back(&setup);
			}
			
			// The vanilla game picks the setup that fills the road's width the most, ignoring ties
			if ((not vanillaResult) or (setup.standard.minRoadWidth > vanillaResult->standard.minRoadWidth))
				vanillaResult = &setup;
		}

		// Attempt to select a random eligible setup
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
					candidates.clear(); // safe due to immediate return

					return setup->GetRandomTable();
				}
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [RBL] Failed to select roadblock setup");

			candidates.clear();
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("No candidate(s)");

		// Fall back to vanilla result (if available)
		if (vanillaResult)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("Best width:", vanillaResult->standard.minRoadWidth);

			return vanillaResult->GetRandomTable();
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

			return false; // no setups; disable feature
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

		std::string name;

		// Attempt to parse each setup
		for (const auto& [section, contents] : sections)
		{
			if (section.find(baseName) > 0) continue; // not setup; parse next

			float minRoadWidth = 0.f;
			float maxRoadWidth = 0.f;

			name = section.substr(baseName.length());

			// Parse and validate width values
			if (not parser.ParseFromFile<float, float>(section, "extent", {minRoadWidth, {0.f}}, {maxRoadWidth, {0.f}}))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', name, "(no extent)");

				continue; // invalid setup; parse next
			}

			if (minRoadWidth >= maxRoadWidth)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', name, "(invalid extent)");

				continue; // invalid setup; parse next
			}

			// Create and initialise global setup object
			RBSetup& setup = roadblockSetups.emplace_back(std::move(name));

			setup.standard.minRoadWidth = minRoadWidth;
			setup.maxRoadWidth          = maxRoadWidth;

			// Parse roadblock-part parameters
			isValids = parser.ParseFormat<maxNumParts, int, float, float, float>
			(
				section,
				partFormat,
				{partTypes},
				{partOffsetsX},
				{partOffsetsY},
				{orientations}
			);

			// Process roadblock parts
			size_t numValidParts = 0;

			for (size_t partID = 0; partID < maxNumParts; ++partID)
			{
				if (not isValids[partID]) continue; // invalid part; process next

				switch (partTypes[partID])
				{
				case RBPartType::CAR:
					++(setup.standard.numCarsRequired);
					break;

				case RBPartType::BLOCKADE:
					break;

				case RBPartType::SPIKES:
					setup.hasSpikes = true;
					break;

				default:
					continue; // invalid part; process next
				}

				// Remove redundant rotations
				orientations[partID] -= static_cast<int>(orientations[partID]);

				if (orientations[partID] < 0.f)
					orientations[partID] += 1.f;

				// Update part parameters
				setup.standard.parts[numValidParts++] =
				{
					static_cast<RBPartType>(partTypes[partID]),
					partOffsetsX[partID],
					partOffsetsY[partID],
					orientations[partID]
				};
			}

			if (setup.standard.numCarsRequired == 0)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', setup.name, "(no car(s))");

				roadblockSetups.pop_back(); // safe due to immediate continue

				continue; // invalid setup; parse next
			}

			// Parse and validate optional parameters
			parser.ParseFromFile<float>(section, "mirrored", {setup.mirrorChance, {0.f, 100.f}});

			HeatParameters::Parse(parser, section, setup.chances);

			if (setup.chances.GetMaximum() < 1)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>('-', setup.name, "(0 chance)");

				roadblockSetups.pop_back(); // safe due to immediate continue

				continue; // unused setup; parse next
			}
			
			// Create mirrored variant (not worth skipping if disabled)
			setup.mirrored = setup.standard;

			for (size_t partID = 0; partID < maxNumParts; ++partID)
			{
				RBPart& part = setup.mirrored.parts[partID];

				if (part.type == RBPartType::NONE) break; // no more parts

				part.offsetX     = -part.offsetX;
				part.orientation = 1.f - part.orientation;
					
				// Mirror spike-strip pattern direction
				if (part.type == RBPartType::SPIKES)
				{
					if (part.orientation > .5f)
						part.orientation -= .5f; // clockwise

					else
						part.orientation += .5f; // counter-clockwise
				}
			}

			// Increment logging counters
			if constexpr (Globals::loggingEnabled)
				counter.CountSetup(setup);
		}

		if (roadblockSetups.empty())
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no setup(s) valid");

			return false; // no valid setups; disable feature
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log<3>(static_cast<int>(roadblockSetups.size()), "setup(s) valid");

			Globals::logger.Log<3>(static_cast<int>(counter.numRegular), "regular,", static_cast<int>(counter.numMirrorRegular), "mirrored");
			Globals::logger.Log<3>(static_cast<int>(counter.numSpike),   "spikes,",  static_cast<int>(counter.numMirrorSpikes),  "mirrored");

			counter.ResetCounts();
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



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [RBL] RoadblockOverrides");

		spawnCalloutChances.Log("spawnCalloutChance      ");
		spikeCalloutChances.Log("spikeCalloutChance      ");

		Globals::logger.Log<2>("numRegularRoadblocks:   ", static_cast<int>(counter.numRegular), static_cast<int>(counter.numMirrorRegular));
		Globals::logger.Log<2>("numSpikeRoadblocks:     ", static_cast<int>(counter.numSpike),   static_cast<int>(counter.numMirrorSpikes));

		counter.ResetCounts();
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
		for (RBSetup& setup : roadblockSetups)
		{
			setup.chances.SetToHeat(isRacing, heatLevel);

			if constexpr (Globals::loggingEnabled)
			{
				if (setup.chances.current > 0)
					counter.CountSetup(setup);
			}
		}

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}