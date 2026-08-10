#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace RoadblockOverrides
{
	// Roadblock (part) structs ---------------------------------------------------------------------------------------------------------------------

	// Part (matches vanilla layout)
	enum RBPartType : int // C-style for implicit casting
	{
		NONE     = 0,
		CAR      = 1,
		SAWHORSE = 2,
		SPIKES   = 3
	};

	struct RBPart
	{
	// Members

		RBPartType type = RBPartType::NONE;

		float offsetX     = 0.f; // metres
		float offsetY     = 0.f; // metres
		float orientation = 0.f; // full rotations
	};

	static_assert(sizeof(RBPart) == 16, "Part-size mismatch");



	// Table (matches vanilla layout)
	constexpr size_t maxNumParts = 6;

	struct RBTable
	{
	// Members

		float  minRoadWidth    = 0.f; // metres
		size_t numCarsRequired = 0;   // cars

		RBPart parts[maxNumParts]; // default-initialised
	};

	static_assert(sizeof(RBTable) == 104, "Table-size mismatch");



	// Setup (mod-specific)
	struct RBSetup
	{
	// Members

		std::string name; // for logging; not worth removing

		RBTable original;
		RBTable mirrored;

		bool hasSpikes  = false;
		bool canStretch = true;

		float maxRoadWidth = 0.f; // metres
		float mirrorChance = 0.f; // percent

		HeatParameters::Value<int> chance{100, {0}}; // relative


	// Methods

		[[nodiscard]] bool IsAvailable() const
		{
			return (this->chance.current > 0);
		}


		[[nodiscard]] bool IsCompatbleRoadWidth(const float roadWidth) const
		{
			return ((roadWidth >= this->original.minRoadWidth) and (roadWidth < this->maxRoadWidth)); // same as mirrored
		}


		[[nodiscard]] size_t GetNumCarsRequired() const
		{
			return this->original.numCarsRequired; // same as mirrored
		}


		[[nodiscard]] bool IsCompatibleCarCount(const size_t maxNumCars) const
		{
			return (this->GetNumCarsRequired() <= maxNumCars);
		}


		[[nodiscard]] const RBTable& GetRandomTable() const
		{
			const bool isMirrored = Globals::prng.DoPercentTrial<float>(this->mirrorChance);

			if constexpr (Globals::loggingEnabled)
			{
				if (isMirrored)
					Globals::logger.Log<2>("Setup:", this->name, "(mirrored)");

				else
					Globals::logger.Log<2>("Setup:", this->name);
			}

			return (isMirrored) ? this->mirrored : this->original;
		}


		[[nodiscard]] float GetMaxStretchScale() const
		{
			return (this->canStretch) ? 1.14f : 1.f;
		}


		[[nodiscard]] bool IsMirrorEnabled() const
		{
			return (this->mirrorChance > 0.f);
		}
	};



	// Counter for logging
	struct SetupCounter
	{
	// Members

		size_t numRegular = 0;
		size_t numSpike   = 0;

		size_t numMirrorRegular = 0;
		size_t numMirrorSpike   = 0;


	// Methods

		void Reset()
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

				if (setup.IsMirrorEnabled())
					++numMirrorSpike;
			}
			else
			{
				++numRegular;

				if (setup.IsMirrorEnabled())
					++numMirrorRegular;
			}
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Aliases
	template <typename T>
	using PartArray = std::array<T, maxNumParts>;

	// Heat parameters
	constinit HeatParameters::Value<float> spawnCalloutChances(100.f, {0.f, 100.f}); // percent
	constinit HeatParameters::Value<float> spikeCalloutChances(50.f,  {0.f, 100.f}); // percent

	// Setup parsing
	constexpr std::string_view setupPrefix = "Setups:";
	
	// Code caves
	RELEASE_CONSTINIT std::vector<RBSetup> roadblockSetups;

	float maxStretchScale = 1.14f;

	bool hasSpikes = false;
	int  spikeLane = 0;

	// Logging
	address roadblockPursuit = 0x0;

	constinit SetupCounter counter;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall RequestCallout(const address pursuit)
	{
		if (Globals::IsPursuitInCooldownMode(pursuit)) return;
		if (not Globals::IsPlayerPursuit(pursuit))     return;

		if (Globals::prng.DoPercentTrial<float>(spawnCalloutChances.current))
		{
			if (hasSpikes and Globals::prng.DoPercentTrial<float>(spikeCalloutChances.current))
			{
				const auto CallOutSpikes = AsFunction<void __cdecl (int)>(0x71DAC0);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(roadblockPursuit, "[RBL] Spikes callout");

				CallOutSpikes(spikeLane);
			}
			else
			{
				const auto CallOutRegular = AsFunction<void ()>(0x71DAA0);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(roadblockPursuit, "[RBL] Regular callout");

				CallOutRegular();
			}
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log(roadblockPursuit, "[RBL] No callout");
	}





	// Replacement functions ------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] size_t __stdcall GetMaxNumCarsRequired(const float roadWidth)
	{
		size_t maxNumCarsRequired = 0;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (not setup.IsAvailable())                   continue;
			if (not setup.IsCompatbleRoadWidth(roadWidth)) continue;

			if (not setup.IsCompatibleCarCount(maxNumCarsRequired))
				maxNumCarsRequired = setup.GetNumCarsRequired();
		}

		return maxNumCarsRequired;
	}



	[[nodiscard]] const RBTable* __cdecl SelectRoadblockTable
	(
		const float  roadWidth, 
		const size_t maxNumCars, 
		const bool   needsSpikes
	) {
		static RELEASE_CONSTINIT std::vector<const RBSetup*> candidates;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log(roadblockPursuit, "[RBL] Roadblock request", (needsSpikes) ? "(spikes)" : "(regular)");

			Globals::logger.Log<2>("Max. cars:",  DecFormat(maxNumCars));
			Globals::logger.Log<2>("Road width:", roadWidth);
		}

		// Find eligible setups
		int totalChance = 0;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (setup.hasSpikes != needsSpikes) continue;

			if (not setup.IsAvailable())                    continue;
			if (not setup.IsCompatbleRoadWidth(roadWidth))  continue;
			if (not setup.IsCompatibleCarCount(maxNumCars)) continue;

			totalChance += setup.chance.current;
			candidates.push_back(&setup);
		}

		// Check setup count
		if (candidates.empty())
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("No candidate(s)");

			return nullptr; // no viable setup(s)
		}

		// Select a random eligible setup
		int       cumulativeChance = 0;
		const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, totalChance);

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>(DecFormat(candidates.size()), "candidate(s)");

		for (const RBSetup* const setup : candidates)
		{
			cumulativeChance += setup->chance.current;
			if (cumulativeChance < chanceThreshold) continue;
			
			const auto* const table = &(setup->GetRandomTable());
			maxStretchScale         = setup->GetMaxStretchScale();
					
			candidates.clear(); // safe due to immediate return

			return table; // use random table
		}

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [RBL] Failed to select roadblock setup");

		candidates.clear();
		
		return nullptr; // should never happen
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address pursuitEntrance = 0x43DD4F;
	constexpr address pursuitExit     = 0x43DD56;

	// Saves the roadblock pursuit for logging purposes
	__declspec(naked) void Pursuit()
	{
		__asm
		{
			// Execute original code first
			mov ecx, dword ptr [esp + 0x4BC]

			mov dword ptr [roadblockPursuit], ecx

			jmp dword ptr [pursuitExit]
		}
	}



	constexpr address spikeLaneEntrance = 0x43E574;
	constexpr address spikeLaneExit     = 0x43E57B;

	// Records the last spike-strip position
	__declspec(naked) void SpikeLane()
	{
		__asm
		{
			mov byte ptr [hasSpikes], 1
			mov dword ptr [spikeLane], eax

			// Execute original code and resume
			lea ecx, dword ptr [esp + 0xA4]

			jmp dword ptr [spikeLaneExit]
		}
	}



	constexpr address scaleLimitEntrance = 0x43E345;
	constexpr address scaleLimitExit     = 0x43E34D;

	// Enforces the maximum stretch-scale for roadblocks
	__declspec(naked) void ScaleLimit()
	{
		__asm
		{
			mov eax, dword ptr [maxStretchScale]
			mov dword ptr [esp + 0x2C], eax

			jmp dword ptr [scaleLimitExit]
		}
	}



	constexpr address maxCarCountEntrance = 0x43DF2A;
	constexpr address maxCarCountExit     = 0x43DF8A;

	// Fetches the maximum car count among eligible setups
	__declspec(naked) void MaxCarCount()
	{
		__asm
		{
			fstp dword ptr [esp + 0x14]

			push dword ptr [esp + 0x14]
			call GetMaxNumCarsRequired
			mov esi, eax

			jmp dword ptr [maxCarCountExit]
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

			mov al, 1 // restore value

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x4B4]

			jmp dword ptr [radioRequestExit]
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
			xor ecx, ecx // restore zero flag
			
			conclusion:
			mov byte ptr [hasSpikes], 0

			jmp dword ptr [spawnFailureExit]
		}
	}




	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] std::optional<RBSetup> ParseRoadblockSetup
	(
		const HeatParameters::Parser& parser,
		const std::string_view        section
	) {
		if (section.find(setupPrefix) > 0) return std::nullopt; // not setup

		RBSetup setup;

		if constexpr (Globals::loggingEnabled)
			setup.name = section.substr(setupPrefix.length());

		RBTable& table = setup.original;

		// Parse and validate width values
		if (not parser.ParseFromFile<float, float>(section, "extent", {table.minRoadWidth, {.001f}}, {setup.maxRoadWidth, {0.f}}))
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
		PartArray<int>   partTypeIDs  = {};
		PartArray<float> partOffsetsX = {};
		PartArray<float> partOffsetsY = {};
		PartArray<float> orientations = {};

		const PartArray<bool> isValids = parser.ParseFormat<maxNumParts, int, float, float, float>
		(
			section,
			{}, // no "default" value(s)
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
			if (not isValids[partID]) continue; // invalid part

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
				continue; // invalid part
			}

			// Remove full rotation(s) and convert to positive value
			orientations[partID] -= std::trunc(orientations[partID]);

			if (orientations[partID] < 0.f)
				orientations[partID] += 1.f; // full rotation

			// Update part parameters
			table.parts[numValidParts++] =
			{
				static_cast<RBPartType>(partTypeIDs[partID]),
				partOffsetsX[partID],
				partOffsetsY[partID],
				orientations[partID]
			};
		}

		// Validate car count
		if (table.numCarsRequired == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>('-', setup.name, "(no car(s))");

			return std::nullopt; // invalid setup
		}

		// Parse and validate "chance" parameter(s)
		HeatParameters::Parse(parser, section, setup.chance);

		if (setup.chance.GetMaximum() < 1)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>('-', setup.name, "(unused)");

			return std::nullopt; // unused setup
		}

		// Parse other optional parameters
		parser.ParseFromFile<bool> (section, "stretch", {setup.canStretch});
		parser.ParseFromFile<float>(section, "mirror",  {setup.mirrorChance, {0.f, 100.f}});

		// Create mirrored table
		setup.mirrored = table;

		for (RBPart& part : setup.mirrored.parts)
		{
			if (part.type == RBPartType::NONE) break; // no more part(s)

			part.offsetX     = -part.offsetX;
			part.orientation = 1.f - part.orientation;

			// Mirror spike-strip pattern
			if (part.type == RBPartType::SPIKES)
			{
				if (part.orientation < .5f)
					part.orientation += .5f; // counter-clockwise

				else
					part.orientation -= .5f; // clockwise
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
			Globals::logger.Log<3>(DecFormat(maxNumSetups), "setup(s) provided");

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

		// Log and shrink setup vector
		if constexpr (Globals::loggingEnabled)
		{
			if (not roadblockSetups.empty())
			{
				Globals::logger.Log<3>(DecFormat(roadblockSetups.size()), "setup(s) valid");

				Globals::logger.Log<3>(DecFormat(counter.numRegular), "regular,", DecFormat(counter.numMirrorRegular), "mirrored");
				Globals::logger.Log<3>(DecFormat(counter.numSpike),   "spikes, ", DecFormat(counter.numMirrorSpike),   "mirrored");

				counter.Reset();
			}
			else Globals::logger.Log<3>("no setup(s) valid");
		}

		roadblockSetups.shrink_to_fit();

		return (not roadblockSetups.empty());
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RBL] RoadblockOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Roadblocks:Radio", spawnCalloutChances, spikeCalloutChances);

		// Roadblock setups
		if (ParseRoadblockSetups(parser))
		{
			// Code changes (conditional)
			MemoryTools::Write<float*>(&maxStretchScale, {0x43E334});

			MemoryTools::MakeRangeNOP<0x43E146, 0x43E14F>(); // strict cop-count check

			MemoryTools::MakeRangeJMP<0x4063D0, 0x40644A>(SelectRoadblockTable); // replaces game function

			MemoryTools::MakeRangeJMP<scaleLimitEntrance,  scaleLimitExit> (ScaleLimit);
			MemoryTools::MakeRangeJMP<maxCarCountEntrance, maxCarCountExit>(MaxCarCount);
		}

		// Code Changes (general)
		MemoryTools::MakeRangeNOP<0x71F184, 0x71F19F>(); // regular callout
		MemoryTools::MakeRangeNOP<0x71F091, 0x71F096>(); // spikes  callout

		MemoryTools::MakeRangeJMP<spikeLaneEntrance,    spikeLaneExit>   (SpikeLane);
		MemoryTools::MakeRangeJMP<radioRequestEntrance, radioRequestExit>(RadioRequest);
		MemoryTools::MakeRangeJMP<spawnFailureEntrance, spawnFailureExit>(SpawnFailure);

		// Code changes (logging)
		if constexpr (Globals::loggingEnabled)
			MemoryTools::MakeRangeJMP<pursuitEntrance, pursuitExit>(Pursuit);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		Globals::logger.Log("    HEAT [RBL] RoadblockOverrides");

		spawnCalloutChances.Log("spawnCalloutChance      ");
		spikeCalloutChances.Log("spikeCalloutChance      ");

		if (roadblockSetups.empty()) return;

		Globals::logger.Log<2>("numRegularRoadblocks    ", DecFormat(counter.numRegular), '/', DecFormat(counter.numMirrorRegular));
		Globals::logger.Log<2>("numSpikeRoadblocks      ", DecFormat(counter.numSpike),   '/', DecFormat(counter.numMirrorSpike));
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		// Heat parameters
		spawnCalloutChances.SetToHeatState(state);
		spikeCalloutChances.SetToHeatState(state);

		// Roadblock setups
		for (RBSetup& setup : roadblockSetups)
		{
			setup.chance.SetToHeatState(state);

			if constexpr (Globals::loggingEnabled)
			{
				if (setup.IsAvailable())
					counter.CountSetup(setup);
			}
		}

		if constexpr (Globals::loggingEnabled)
		{
			LogHeatStateReport();
			counter.Reset();
		}
	}
}