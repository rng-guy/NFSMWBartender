#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <format>
#include <utility>
#include <optional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace RoadblockOverrides
{

	// Roadblock (part) structs ---------------------------------------------------------------------------------------------------------------------

	// Parts (matches vanilla layout)
	enum RBPartType
	{
		NONE     = 0,
		CAR      = 1,
		SAWHORSE = 2,
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



	// Table (matches vanilla layout)
	constexpr size_t maxNumParts = 6;

	struct RBTable
	{
		float  minRoadWidth    = 0.f; // metres
		size_t numCarsRequired = 0;   // cars

		RBPart parts[maxNumParts]; // default-initialised
	};

	static_assert(sizeof(RBTable) == 104, "Table size mismatch");



	// Setup (mod-specific)
	struct RBSetup
	{
		std::string name;

		RBTable standard;
		RBTable mirrored;

		bool hasSpikes  = false;
		bool canStretch = true;

		float maxRoadWidth = 0.f;  // metres
		float mirrorChance = 50.f; // percent

		HeatParameters::Pair<int> chances{100, {0}}; // relative


		const RBTable* GetRandomTable() const
		{
			const bool isMirrored = Globals::prng.DoPercentTrial<float>(this->mirrorChance);

			if constexpr (Globals::loggingEnabled)
			{
				if (isMirrored)
					Globals::logger.Log<2>("Setup:", this->name, "(mirrored)");

				else
					Globals::logger.Log<2>("Setup:", this->name);
			}

			return &((isMirrored) ? this->mirrored : this->standard);
		}


		float GetMaxStretchScale() const
		{
			return (this->canStretch) ? 1.14f : 1.f;
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
		size_t numMirrorSpike   = 0;


		void ResetCounts()
		{
			this->numRegular = 0;
			this->numSpike   = 0;

			this->numMirrorRegular = 0;
			this->numMirrorSpike   = 0;
		}


		void CountSetup(const RBSetup& setup)
		{
			if (setup.hasSpikes)
			{
				++numSpike;

				if (setup.MirrorEnabled())
					++numMirrorSpike;
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
	constinit HeatParameters::Pair<float> spawnCalloutChances(100.f, {0.f, 100.f}); // percent
	constinit HeatParameters::Pair<float> spikeCalloutChances(50.f,  {0.f, 100.f}); // percent

	// Setup parsing
	constexpr std::string_view setupPrefix = "Setups:";
	
	// Code caves
	constinit std::vector<RBSetup> roadblockSetups;

	float maxStretchScale = 1.14f;

	bool hasSpikes = false;
	int  spikeLane = 0;

	// Logging
	constinit SetupCounter counter;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall RequestCallout(const address pursuit)
	{
		if (Globals::IsInCooldownMode(pursuit))    return;
		if (not Globals::IsPlayerPursuit(pursuit)) return;

		if (Globals::prng.DoPercentTrial<float>(spawnCalloutChances.current))
		{
			if (hasSpikes and Globals::prng.DoPercentTrial<float>(spikeCalloutChances.current))
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
		static constinit std::vector<const RBSetup*> candidates;

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
			int cumulativeChance = 0;

			const int chanceThreshold = Globals::prng.GenerateNumber<int>(1, totalChance);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(static_cast<int>(candidates.size()), "candidate(s)");

			for (const RBSetup* const setup : candidates)
			{
				cumulativeChance += setup->chances.current;

				if (cumulativeChance >= chanceThreshold)
				{
					candidates.clear(); // safe due to immediate return

					maxStretchScale = setup->GetMaxStretchScale();

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

			maxStretchScale = vanillaResult->GetMaxStretchScale();

			return vanillaResult->GetRandomTable();
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("No best fit");

		return nullptr;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address spikeLaneEntrance = 0x43E574;
	constexpr address spikeLaneExit     = 0x43E57B;

	// Records the last spike-strip position
	__declspec(naked) void SpikeLane()
	{
		__asm
		{
			mov byte ptr hasSpikes, 0x1
			mov dword ptr spikeLane, eax

			// Execute original code and resume
			lea ecx, dword ptr [esp + 0xA4]

			jmp dword ptr spikeLaneExit
		}
	}



	constexpr address scaleLimitEntrance = 0x43E345;
	constexpr address scaleLimitExit     = 0x43E34D;

	// Enforces the maximum stretch-scale for roadblocks
	__declspec(naked) void ScaleLimit()
	{
		__asm
		{
			mov eax, dword ptr maxStretchScale
			mov dword ptr [esp + 0x2C], eax

			jmp dword ptr scaleLimitExit
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




	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	std::optional<RBSetup> ParseRoadblockSetup
	(
		const HeatParameters::Parser& parser,
		const std::string_view        section
	) {
		if (section.find(setupPrefix) > 0) return std::nullopt; // not setup

		RBSetup setup(std::string(section.substr(setupPrefix.length())));

		RBTable& table = setup.standard;

		// Parse and validate width values
		if (not parser.ParseFromFile<float, float>(section, "extent", {table.minRoadWidth, {0.f}}, {setup.maxRoadWidth, {0.f}}))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>('-', setup.name, "(no extent)");

			return std::nullopt; // invalid setup
		}

		if (table.minRoadWidth >= setup.maxRoadWidth)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>('-', setup.name, "(invalid extent)");

			return std::nullopt; // invalid setup
		}

		// Parse roadblock-part parameters
		std::array<int,   maxNumParts> partTypeIDs  = {};
		std::array<float, maxNumParts> partOffsetsX = {};
		std::array<float, maxNumParts> partOffsetsY = {};
		std::array<float, maxNumParts> orientations = {};

		const auto isValids = parser.ParseFormat<maxNumParts, int, float, float, float>
		(
			section,
			{}, // no "default" value tuple
			"part{:02}",
			HeatParameters::configFormatStart,
			{partTypeIDs},
			{partOffsetsX},
			{partOffsetsY},
			{orientations}
		);

		// Process roadblock parts
		size_t numValidParts = 0;

		for (size_t partID = 0; partID < maxNumParts; ++partID)
		{
			if (not isValids[partID]) continue; // invalid part; process next

			switch (partTypeIDs[partID])
			{
			case RBPartType::CAR:
				++(table.numCarsRequired);
				break;

			case RBPartType::SAWHORSE:
				break;

			case RBPartType::SPIKES:
				setup.hasSpikes = true;
				break;

			default:
				continue; // invalid part; process next
			}

			// Remove full rotations and convert to positive orientation
			orientations[partID] -= std::trunc(orientations[partID]);

			if (orientations[partID] < 0.f)
				orientations[partID] += 1.f;

			// Update part parameters
			table.parts[numValidParts++] =
			{
				static_cast<RBPartType>(partTypeIDs[partID]),
				partOffsetsX[partID],
				partOffsetsY[partID],
				orientations[partID]
			};
		}

		if (table.numCarsRequired == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>('-', setup.name, "(no car(s))");

			return std::nullopt; // invalid setup
		}

		// Parse and validate "chance" parameter(s)
		HeatParameters::Parse(parser, section, setup.chances);

		if (setup.chances.GetMaximum() < 1)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>('-', setup.name, "(unused)");

			return std::nullopt; // unused setup
		}

		// Parse other optional parameters
		parser.ParseFromFile<bool> (section, "stretch", {setup.canStretch});
		parser.ParseFromFile<float>(section, "mirror",  {setup.mirrorChance, {0.f, 100.f}});

		// Create mirrored variant (not worth skipping if disabled)
		setup.mirrored = table;

		for (size_t partID = 0; partID < maxNumParts; ++partID)
		{
			RBPart& part = setup.mirrored.parts[partID];

			if (part.type == RBPartType::NONE) break; // no more parts

			part.offsetX = -part.offsetX;
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

		return setup;
	}



	bool ParseRoadblockSetups(const HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("Roadblock setups:");

		const auto& sections = parser.GetSections();

		// Check (potential) setup count
		size_t maxNumSetups = 0;

		for (const auto& [section, contents] : sections)
			maxNumSetups += (section.find(setupPrefix) == 0);

		if (maxNumSetups == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no setup(s) provided");

			return false; // no setups; disable feature
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<3>(static_cast<int>(maxNumSetups), "setup(s) provided");

		// Parse and validate setups
		roadblockSetups.reserve(maxNumSetups);

		for (const auto& [section, contents] : sections)
		{
			if (auto setup = ParseRoadblockSetup(parser, section))
			{
				if constexpr (Globals::loggingEnabled)
					counter.CountSetup(*setup);

				roadblockSetups.push_back(std::move(*setup));
			}
		}

		// Log parsing result(s)
		if constexpr (Globals::loggingEnabled)
		{
			if (not roadblockSetups.empty())
			{
				roadblockSetups.shrink_to_fit();

				Globals::logger.Log<3>(static_cast<int>(roadblockSetups.size()), "setup(s) valid");

				Globals::logger.Log<3>(static_cast<int>(counter.numRegular), "regular,", static_cast<int>(counter.numMirrorRegular), "mirrored");
				Globals::logger.Log<3>(static_cast<int>(counter.numSpike),   "spikes, ", static_cast<int>(counter.numMirrorSpike),   "mirrored");

				counter.ResetCounts();
			}
			else Globals::logger.Log<3>("no setup(s) valid");
		}

		return (not roadblockSetups.empty());
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Fixes cop-spawn stalling due to failed roadblock spawn attempts
		MemoryTools::MakeRangeJMP<spawnFailureEntrance, spawnFailureExit>(SpawnFailure);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RBL] RoadblockOverrides");

		if (not parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Roadblocks:Radio", spawnCalloutChances, spikeCalloutChances);

		// Roadblock setups
		if (not ParseRoadblockSetups(parser)) return false; // no valid setups; disable feature

		// Code Changes
		MemoryTools::Write<float*>(&maxStretchScale, {0x43E334});

		MemoryTools::MakeRangeNOP<0x71F184, 0x71F19F>(); // regular callout
		MemoryTools::MakeRangeNOP<0x71F091, 0x71F096>(); // spikes  callout

		MemoryTools::MakeRangeJMP<0x4063D0, 0x40644A>(SelectRoadblockTable);

		MemoryTools::MakeRangeJMP<spikeLaneEntrance,    spikeLaneExit>   (SpikeLane);
		MemoryTools::MakeRangeJMP<scaleLimitEntrance,   scaleLimitExit>  (ScaleLimit);
		MemoryTools::MakeRangeJMP<radioRequestEntrance, radioRequestExit>(RadioRequest);

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

		Globals::logger.Log<2>("numRegularRoadblocks    ", static_cast<int>(counter.numRegular), '/', static_cast<int>(counter.numMirrorRegular));
		Globals::logger.Log<2>("numSpikeRoadblocks      ", static_cast<int>(counter.numSpike),   '/', static_cast<int>(counter.numMirrorSpike));

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