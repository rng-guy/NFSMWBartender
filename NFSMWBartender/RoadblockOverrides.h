#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <utility>
#include <optional>
#include <algorithm>
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

		RBTable original;
		RBTable mirrored;

		bool hasSpikes  = false;
		bool canStretch = true;

		float maxRoadWidth = 0.f; // metres
		float mirrorChance = 0.f; // percent

		HEAT_PARAMETER_VALUE(int, chance, 100, {0}); // relative

		[[no_unique_address]] const LogString name;


	// Methods

		explicit RBSetup(const std::string_view name) : name(name) {}


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
					Globals::LogPlain("Setup:", this->name, "(mirrored)");

				else Globals::LogPlain("Setup:", this->name);
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





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[RBL]";
	constexpr LogLiteral logName = "RoadblockOverrides";

	// Types and aliases
	template <typename T>
	using PartArray = std::array<T, maxNumParts>;

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(float, spawnCalloutChance, 100.f, {0.f, 100.f}); // percent
	constinit HEAT_PARAMETER_VALUE(float, spikeCalloutChance, 50.f,  {0.f, 100.f}); // percent

	// Setup parsing
	constexpr std::string_view setupPrefix = "Setups:";
	
	// Code caves
	RELEASE_CONSTINIT std::vector<RBSetup> roadblockSetups;

	size_t numRegularCandidates = 0;
	size_t numSpikeCandidates   = 0;
	size_t maxNumCarsRequired   = 0;

	float maxStretchScale = 1.14f;

	bool hasSpikes = false;
	int  spikeLane = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] void __stdcall GatherSetupMetadata(const float roadWidth)
	{
		numRegularCandidates = 0;
		numSpikeCandidates   = 0;
		maxNumCarsRequired   = 0;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (not setup.IsAvailable())                   continue;
			if (not setup.IsCompatbleRoadWidth(roadWidth)) continue;

			if (setup.hasSpikes) ++numSpikeCandidates;
			else                 ++numRegularCandidates;

			maxNumCarsRequired = std::max<size_t>(maxNumCarsRequired, setup.GetNumCarsRequired());
		}
	}



	[[nodiscard]] bool __fastcall WasRequestFeasible(const address pursuit)
	{
		const bool anyRegular = (numRegularCandidates > 0);
		const bool anySpikes  = (numSpikeCandidates   > 0);

		if (anyRegular and anySpikes)      return true;  // both    available
		if (not (anyRegular or anySpikes)) return false; // neither available

		if (Globals::IsPursuitInCooldownMode(pursuit)) return anyRegular;

		const address attribute   = Globals::GetFromPursuitlevel(pursuit, "roadblockspikechance"_vlt);
		const float   spikeChance = (attribute) ? AsReference<float>(attribute) : 0.f; // should never fail

		if (spikeChance <= 0.f)   return anyRegular; // always regular
		if (spikeChance >= 100.f) return anySpikes;  // always spike

		return true; // either available
	}



	void __fastcall CancelRequest
	(
		const address caller, 
		const address pursuit
	) {
		if constexpr (Globals::loggingEnabled)
			Globals::LogFull(pursuit, logTag, "Cancelling request");

		switch (caller)
		{
		case 0x43E7D6: // HeavyStrategy 4
			Globals::ClearSupportRequest(pursuit);
			break;

		case 0x43EC3A: // non-Strategy roadblock
			AsReference<bool>   (pursuit + 0x190)            = false; // request status
			AsReference<address>(Globals::copManager + 0xBC) = 0x0;   // roadblock pursuit
			AsReference<int>    (Globals::copManager + 0xB8) = 0;     // car count
		}
	}
	


	void __fastcall RequestCallout(const address pursuit)
	{
		if (Globals::IsPursuitInCooldownMode(pursuit)) return;
		if (not Globals::IsPlayerPursuit(pursuit))     return;

		if (not Globals::prng.DoPercentTrial<float>(spawnCalloutChance.current))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogTagged(logTag, "No callout");

			return; // skip callout
		}

		if (hasSpikes and Globals::prng.DoPercentTrial<float>(spikeCalloutChance.current))
		{
			const auto CallOutSpikes = AsFunction<void __cdecl (int)>(0x71DAC0);

			if constexpr (Globals::loggingEnabled)
				Globals::LogTagged(logTag, "Spikes callout");

			CallOutSpikes(spikeLane);
		}
		else
		{
			const auto CallOutRegular = AsFunction<void ()>(0x71DAA0);

			if constexpr (Globals::loggingEnabled)
				Globals::LogTagged(logTag, "Regular callout");

			CallOutRegular();
		}
	}



	[[nodiscard]] auto CountAvailableSetups()
	{
		// Aggregate return type
		struct Counts
		{
		// Members

			size_t numRegular = 0;
			size_t numSpikes  = 0;

			size_t numMirrorRegular = 0;
			size_t numMirrorSpikes  = 0;
		};

		Counts counts;

		// Setup counting
		for (const RBSetup& setup : roadblockSetups)
		{
			if (not setup.IsAvailable()) continue;

			size_t& numSetups = (setup.hasSpikes) ? counts.numSpikes       : counts.numRegular;
			size_t& numMirror = (setup.hasSpikes) ? counts.numMirrorSpikes : counts.numMirrorRegular;

			numSetups += 1;
			numMirror += setup.IsMirrorEnabled();
		}

		return counts;
	}





	// Replacement functions ------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const RBTable* __cdecl SelectRoadblockTable
	(
		const float  roadWidth, 
		const size_t maxNumCars, 
		const bool   needsSpikes
	) {
		static RELEASE_CONSTINIT std::vector<const RBSetup*> candidates;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogTagged(logTag, "Roadblock request", (needsSpikes) ? "(spikes)" : "(regular)");

			Globals::LogPlain("Car budget:", DecFormat(maxNumCars));
			Globals::LogPlain("Road width:", roadWidth);
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
				Globals::LogPlain("No candidate(s)");

			return nullptr; // no viable setup(s)
		}

		// Select a random eligible setup
		int       cumulativeChance = 0;
		const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, totalChance);

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain(DecFormat(candidates.size()), "candidate(s)");

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
			Globals::LogError(logTag, "Failed to select roadblock setup");

		candidates.clear();
		
		return nullptr; // should never happen
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address spikeFlagEntrance = 0x43E1C5;
	constexpr address spikeFlagExit     = 0x43E1CD;

	// Records whether a roadblock has spikes
	__declspec(naked) void SpikeFlag()
	{
		__asm
		{
			// Execute original code first
			mov eax, dword ptr [esp + 0x10]
			mov ecx, dword ptr [esp + 0x14]

			mov byte ptr [hasSpikes], al

			jmp dword ptr [spikeFlagExit]
		}
	}



	constexpr address spikeLaneEntrance = 0x43E574;
	constexpr address spikeLaneExit     = 0x43E57B;

	// Records the last spike-strip position
	__declspec(naked) void SpikeLane()
	{
		__asm
		{
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

	// Finds the maximum car count among eligible setups
	__declspec(naked) void MaxCarCount()
	{
		__asm
		{
			fstp dword ptr [esp + 0x14]

			push dword ptr [esp + 0x14] // roadWidth
			call GatherSetupMetadata

			mov esi, dword ptr [maxNumCarsRequired]

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

	// Prevents impossible roadblock requests from stalling cop spawns
	__declspec(naked) void SpawnFailure()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esp + 0x18], ecx
			test ecx, ecx
			jne conclusion // found setup

			mov ecx, dword ptr [esp + 0x4C4]
			call WasRequestFeasible // ecx: pursuit
			cmp al, 1
			je conclusion          // do not cancel

			mov ecx, dword ptr [esp + 0x4C0]
			mov edx, dword ptr [esp + 0x4C4]
			call CancelRequest // ecx: caller; edx: pursuit

			xor ecx, ecx // restore zero flag
			
			conclusion:
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

		RBSetup setup(section.substr(setupPrefix.length()));

		RBTable& table = setup.original; // same constraints as mirrored

		// Parse and validate width values
		if (not parser.ParseFromFile<float, float>(section, "extent", {table.minRoadWidth, {.001f}}, {setup.maxRoadWidth, {0.f}}))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(no extent)");

			return std::nullopt; // invalid setup
		}

		if (table.minRoadWidth >= setup.maxRoadWidth)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(invalid extent)");

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
			orientations[partID] -= std::floor(orientations[partID]);

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
				Globals::LogDetail('-', setup.name, "(no car(s))");

			return std::nullopt; // invalid setup
		}

		// Parse and validate "chance" parameter(s)
		HeatParameters::Parse(parser, section, setup.chance);

		if (setup.chance.GetMaximum() < 1)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(unused)");

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

			// Mirror spike-strip direction
			if (part.type == RBPartType::SPIKES)
				part.orientation = std::fmod(part.orientation + .5f, 1.f);
		}

		return setup;
	}



	bool ParseRoadblockSetups(const HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Roadblock setups:");

		const auto& sections = parser.GetSections();

		// Check (potential) setup count
		size_t maxNumSetups = 0;

		for (const auto& [section, contents] : sections)
			maxNumSetups += (section.find(setupPrefix) == 0);

		if (maxNumSetups == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail("no setup(s) provided");

			return false; // no setups; disable feature
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::LogDetail(DecFormat(maxNumSetups), "setup(s) provided");

		// Parse and validate setups
		roadblockSetups.reserve(maxNumSetups);

		for (const auto& [section, contents] : sections)
		{
			if (auto setup = ParseRoadblockSetup(parser, section))
				roadblockSetups.push_back(std::move(*setup));
		}

		// Log and shrink setup vector
		if constexpr (Globals::loggingEnabled)
		{
			if (not roadblockSetups.empty())
			{
				Globals::LogDetail(DecFormat(roadblockSetups.size()), "setup(s) valid");

				const auto [numRegular, numSpikes, numMirrorRegular, numMirrorSpikes] = CountAvailableSetups();

				Globals::LogDetail(DecFormat(numRegular), "regular,", DecFormat(numMirrorRegular), "mirrored");
				Globals::LogDetail(DecFormat(numSpikes),  "spikes, ", DecFormat(numMirrorSpikes),  "mirrored");
			}
			else Globals::LogDetail("no setup(s) valid");
		}

		roadblockSetups.shrink_to_fit();

		return (not roadblockSetups.empty());
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Roadblocks:Radio", spawnCalloutChance, spikeCalloutChance);

		// Roadblock setups
		if (ParseRoadblockSetups(parser))
		{
			// Code changes (conditional)
			MemoryTools::Write<float*>(&maxStretchScale, {0x43E334});

			MemoryTools::MakeRangeNOP<0x43E146, 0x43E14F>(); // strict cop-count check

			MemoryTools::MakeRangeJMP<0x4063D0, 0x40644A>(SelectRoadblockTable); // replaces game function

			MemoryTools::MakeRangeJMP<scaleLimitEntrance,   scaleLimitExit>  (ScaleLimit);
			MemoryTools::MakeRangeJMP<maxCarCountEntrance,  maxCarCountExit> (MaxCarCount);
			MemoryTools::MakeRangeJMP<spawnFailureEntrance, spawnFailureExit>(SpawnFailure);
		}

		// Code Changes (general)
		MemoryTools::MakeRangeNOP<0x71F184, 0x71F19F>(); // regular callout
		MemoryTools::MakeRangeNOP<0x71F091, 0x71F096>(); // spikes  callout

		MemoryTools::MakeRangeJMP<spikeFlagEntrance,    spikeFlagExit>   (SpikeFlag);
		MemoryTools::MakeRangeJMP<spikeLaneEntrance,    spikeLaneExit>   (SpikeLane);
		MemoryTools::MakeRangeJMP<radioRequestEntrance, radioRequestExit>(RadioRequest);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		// Heat parameters
		spawnCalloutChance.SetToHeatState(state);
		spikeCalloutChance.SetToHeatState(state);

		// Roadblock setups
		for (RBSetup& setup : roadblockSetups)
			setup.chance.SetToHeatStateWithoutLog(state);

		if constexpr (Globals::loggingEnabled)
		{
			if (roadblockSetups.empty()) return;

			const auto [numRegular, numSpikes, numMirrorRegular, numMirrorSpikes] = CountAvailableSetups();

			Globals::LogPlain("numRegularRoadblocks    ", DecFormat(numRegular), '/', DecFormat(numMirrorRegular));
			Globals::LogPlain("numSpikeRoadblocks      ", DecFormat(numSpikes),  '/', DecFormat(numMirrorSpikes));
		}
	}
}