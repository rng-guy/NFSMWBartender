#pragma once

#include <cmath>
#include <array>
#include <limits>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace RoadblockOverrides
{
	// Vanilla types --------------------------------------------------------------------------------------------------------------------------------

	enum class RBPartType : int
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

		float offsetX = 0.f; // metres
		float offsetY = 0.f; // metres

		float orientation = 0.f; // full rotations
	};

	static_assert(sizeof(RBPart) == 16, "Part-size mismatch");



	constexpr size_t maxNumParts = 6;
	
	struct RBTable
	{
	// Members

		float  minRoadWidth    = 0.f; // metres
		size_t numCarsRequired = 0;   // cars

		RBPart parts[maxNumParts]; // C-style for game compatibility
	};

	static_assert(sizeof(RBTable) == 104, "Table-size mismatch");





	// RBSetup class --------------------------------------------------------------------------------------------------------------------------------

	class RBSetup
	{
	private: // friends

		friend bool ExtractRoadblockSetup(const auto&, RBSetup&);


	private: // members

		bool hasSpikes  = false;
		bool canStretch = true;

		float maxRoadWidth = 0.f; // metres
		float mirrorChance = 0.f; // percent

		RBTable original; // same constraints as mirrored
		RBTable mirrored; // same constraints as original

		HEAT_PARAMETER_VALUE(int, chance, 100, {0}); // relative

		[[no_unique_address]] Globals::LogString name;

	
	public: // methods

		explicit RBSetup(const std::string_view name) : name(name) {}


		void SetToHeatState(const HeatParameters::HeatState state)
		{
			this->chance.SetToHeatStateSilently(state);
		}


		[[nodiscard]] bool IsAvailable() const
		{
			return (this->chance.current > 0);
		}


		[[nodiscard]] int GetChance() const
		{
			return this->chance.current;
		}


		[[nodiscard]] bool HasSpikes() const
		{
			return this->hasSpikes;
		}


		[[nodiscard]] size_t GetNumCarsRequired() const
		{
			return this->original.numCarsRequired;
		}


		[[nodiscard]] bool IsCompatibleCarCount(const size_t maxNumCars) const
		{
			return (this->GetNumCarsRequired() <= maxNumCars);
		}


		[[nodiscard]] bool IsCompatbleRoadWidth(const float roadWidth) const
		{
			return ((roadWidth >= this->original.minRoadWidth) and (roadWidth < this->maxRoadWidth));
		}


		[[nodiscard]] const RBTable& GetRandomTable() const
		{
			const bool isMirrored = Globals::prng.DoPercentTrial<float>(this->mirrorChance);

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("Setup:", this->name, (isMirrored) ? "(mirrored)" : "(regular)");
			
			return (isMirrored) ? this->mirrored : this->original;
		}


		[[nodiscard]] float GetMaxStretchScale() const
		{
			return (this->canStretch) ? 1.14f : 1.f;
		}


		[[nodiscard]] bool IsMirrorable() const
		{
			return (this->mirrorChance > 0.f);
		}


		[[nodiscard]] const auto& GetName() const
		{
			return this->name;
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[RBL]";
	constexpr Globals::LogLiteral logName = "RoadblockOverrides";

	// Aliases
	template <typename T>
	using PartDataArray = std::array<T, maxNumParts>;

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, mayRecycleDistantCops, true);

	constinit HEAT_PARAMETER_VALUE(float, spawnCalloutChance, 100.f, {0.f, 100.f}); // percent
	constinit HEAT_PARAMETER_VALUE(float, spikeCalloutChance, 50.f,  {0.f, 100.f}); // percent

	// Custom roadblock setups
	RELEASE_CONSTINIT std::vector<RBSetup> roadblockSetups;

	// Code caves
	address requestPursuit = 0x0;

	size_t minNumCars = 0;
	size_t maxNumCars = 0;

	float maxStretchScale = 1.14f;

	bool hasSpikes = false;
	int  spikeLane = 0;

	



	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] float GetRoadblockSpikeChance(const address pursuit)
	{
		const float* const spikeChance = AsPointer<float>(Globals::GetFromPursuitLevel(pursuit, "roadblockspikechance"_vlt));

		if (not spikeChance)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Invalid roadblockspikechance pointer in", pursuit);

			ASSERT_UNREACHABLE_THEN(return 0.f);
		}

		return *spikeChance;
	}



	[[nodiscard]] bool IsCustomRequestFeasible
	(
		const bool anyRegular,
		const bool anySpike
	) {
		if (anyRegular and anySpike)      return true;  // both    available
		if (not (anyRegular or anySpike)) return false; // neither available

		if (Globals::IsPursuitInCooldownMode(requestPursuit)) return anyRegular;

		const float spikeChance = GetRoadblockSpikeChance(requestPursuit);

		if (spikeChance <= 0.f)   return anyRegular; // always regular
		if (spikeChance >= 100.f) return anySpike;   // always spike

		return true; // either available
	}



	[[nodiscard]] bool ShouldCustomRequestGetSpikes
	(
		const bool anyRegular,
		const bool anySpike
	) {
		if (not (anyRegular or anySpike))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Incompatible request in", requestPursuit);

			ASSERT_UNREACHABLE_THEN(return false); // will fail anyway
		}

		if (Globals::IsPursuitInCooldownMode(requestPursuit)) return false;

		const float spikeChance = GetRoadblockSpikeChance(requestPursuit);

		if (spikeChance <= 0.f)   return false; // never spikes
		if (spikeChance >= 100.f) return true;  // always spikes

		if (not anyRegular) return true;  // regular impossible
		if (not anySpike)   return false; // spikes  impossible

		return Globals::prng.DoPercentTrial<float>(spikeChance); // either possible
	}



	[[nodiscard]] bool __stdcall UpdateAndAssessCustomRequest
	(
		const address pursuit, 
		const float   roadWidth
	) {
		requestPursuit = pursuit;

		struct RequestReport
		{
			size_t numCandidates = 0;

			size_t minNumCars = std::numeric_limits<size_t>::max();
			size_t maxNumCars = std::numeric_limits<size_t>::min();
		};

		RequestReport regular;
		RequestReport spike;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (not setup.IsAvailable())                   continue;
			if (not setup.IsCompatbleRoadWidth(roadWidth)) continue;

			auto& report = (setup.HasSpikes()) ? spike : regular;

			++(report.numCandidates);

			const size_t numCars = setup.GetNumCarsRequired();

			report.minNumCars = std::min<size_t>(report.minNumCars, numCars);
			report.maxNumCars = std::max<size_t>(report.maxNumCars, numCars);
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogFull(requestPursuit, logTag, "Roadblock creation attempt");

			Globals::LogPlain("Road width:", roadWidth);
			Globals::LogPlain("Candidates:", Globals::LogDec(regular.numCandidates), '/', Globals::LogDec(spike.numCandidates));
		}

		const bool anyRegular = (regular.numCandidates > 0);
		const bool anySpike   = (spike  .numCandidates > 0);

		if (not IsCustomRequestFeasible(anyRegular, anySpike)) return false; // cancel request

		hasSpikes = ShouldCustomRequestGetSpikes(anyRegular, anySpike);

		const auto& report = (hasSpikes) ? spike : regular;

		minNumCars = report.minNumCars;
		maxNumCars = report.maxNumCars;

		return true; // continue request
	}



	void __fastcall CancelCustomRequest(const address caller) 
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Cancelling request");

		switch (caller)
		{
		case 0x43E7D6: // HeavyStrategy 4
			Globals::ClearSupportRequest(requestPursuit);
			return;

		case 0x43EC3A: // non-Strategy roadblock
			AsReference<bool>   (requestPursuit      + 0x190) = false; // request status
			AsReference<address>(Globals::copManager + 0xBC)  = 0x0;   // roadblock pursuit
			AsReference<int>    (Globals::copManager + 0xB8)  = 0;     // car count
			return;
		}

		if constexpr (Globals::loggingEnabled)
			Globals::LogWarning(logTag, "Unknown CreateRoadblock caller:", caller);

		ASSERT_UNREACHABLE;
	}
	


	void __fastcall RequestRoadblockCallout(const address pursuit)
	{
		if (Globals::IsPursuitInCooldownMode(pursuit)) return;
		if (not Globals::IsPlayerPursuit(pursuit))     return;

		if (not Globals::prng.DoPercentTrial<float>(spawnCalloutChance.current))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "No callout");

			return; // skip callout
		}

		if (hasSpikes and Globals::prng.DoPercentTrial<float>(spikeCalloutChance.current))
		{
			const auto CallOutSpikes = AsFunction<void __cdecl (int)>(0x71DAC0);

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "Spike callout");

			CallOutSpikes(spikeLane);
		}
		else
		{
			const auto CallOutRegular = AsFunction<void ()>(0x71DAA0);

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "Regular callout");

			CallOutRegular();
		}
	}



	[[nodiscard]] auto CountAvailableSetups()
	{
		struct Counts
		{
		// Members

			size_t numRegular = 0;
			size_t numSpikes  = 0;

			size_t numMirrorRegular = 0;
			size_t numMirrorSpikes  = 0;
		};

		// Setup counting
		Counts counts;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (not setup.IsAvailable()) continue;

			const bool hasSpikes = setup.HasSpikes();

			size_t& numSetups = (hasSpikes) ? counts.numSpikes       : counts.numRegular;
			size_t& numMirror = (hasSpikes) ? counts.numMirrorSpikes : counts.numMirrorRegular;

			numSetups += 1;
			numMirror += setup.IsMirrorable();
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
			Globals::LogFull(requestPursuit, logTag, "Selecting", (needsSpikes) ? "spike" : "regular", "setup");

			Globals::LogPlain("Car budget:", Globals::LogDec(maxNumCars));
		}

		// Find eligible setups
		int totalChance = 0;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (setup.HasSpikes() != needsSpikes) continue;

			if (not setup.IsAvailable())                    continue;
			if (not setup.IsCompatbleRoadWidth(roadWidth))  continue;
			if (not setup.IsCompatibleCarCount(maxNumCars)) continue;

			totalChance += setup.GetChance();
			candidates.push_back(&setup);
		}

		// Check setup count
		if (candidates.empty())
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "No suitable candidate(s)");

			ASSERT_UNREACHABLE_THEN(return nullptr);
		}

		// Select a random eligible setup
		int       cumulativeChance = 0;
		const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, totalChance);

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain(Globals::LogDec(candidates.size()), "candidate(s)");

		for (const RBSetup* const setup : candidates)
		{
			cumulativeChance += setup->GetChance();
			if (cumulativeChance < chanceThreshold) continue;

			const auto* const table = &(setup->GetRandomTable());
			maxStretchScale         = setup->GetMaxStretchScale();

			candidates.clear(); // safe due to immediate return

			return table; // use random table
		}

		candidates.clear();

		if constexpr (Globals::loggingEnabled)
			Globals::LogWarning(logTag, "Failed to select roadblock setup");

		ASSERT_UNREACHABLE_THEN(return nullptr);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

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



	constexpr address spikeCheckEntrance = 0x43E1C5;
	constexpr address spikeCheckExit     = 0x43E1CD;

	// Records whether the roadblock needs spikes
	__declspec(naked) void SpikeCheck()
	{
		__asm
		{
			// Execute original code first
			mov eax, dword ptr [esp + 0x10]
			mov ecx, dword ptr [esp + 0x14]

			mov byte ptr [hasSpikes], al

			jmp dword ptr [spikeCheckExit]
		}
	}


	
	constexpr address customScaleEntrance = 0x43E345;
	constexpr address customScaleExit     = 0x43E34D;

	// Enforces the maximum stretch-scale of the custom roadblock
	__declspec(naked) void CustomScale()
	{
		__asm
		{
			mov eax, dword ptr [maxStretchScale]
			mov dword ptr [esp + 0x2C], eax

			jmp dword ptr [customScaleExit]
		}
	}



	constexpr address copRecyclingEntrance = 0x43E0EF;
	constexpr address copRecyclingExit     = 0x43E0F5;

	// Decides whether to recycle cops for the roadblock
	__declspec(naked) void CopRecycling()
	{
		__asm
		{
			// Execute original code first
			mov eax, esi
			sub eax, ecx
			test eax, eax
			jle conclusion // no recycleable cops

			cmp byte ptr [mayRecycleDistantCops.current], 0

			conclusion:
			jmp dword ptr [copRecyclingExit]
		}
	}



	constexpr address requestOutcomeEntrance = 0x43E20C;
	constexpr address requestOutcomeExit     = 0x43E213;

	// Processes the outcome of the roadblock request
	__declspec(naked) void RequestOutcome()
	{
		__asm
		{
			test al, al
			je conclusion // request failed

			mov ecx, dword ptr [esp + 0x4C4]
			call RequestRoadblockCallout // ecx: pursuit

			mov al, 1 // restore value

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x4B4]

			jmp dword ptr [requestOutcomeExit]
		}
	}



	constexpr address customCarBudgetEntrance = 0x43E146;
	constexpr address customCarBudgetExit     = 0x43E1C5;

	// Checks the required car budget for the custom request
	__declspec(naked) void CustomCarBudget()
	{
		static constexpr address customCarBudgetSkip = 0x43E1E2;

		__asm
		{
			cmp ebp, dword ptr [minNumCars]
			jl skip // insufficient car budget

			mov al, byte ptr [hasSpikes]
			mov byte ptr [esp + 0x10], al

			// Execute original code and resume
			mov esi, dword ptr [esp + 0x4C4]
			xor edi, edi

			jmp dword ptr [customCarBudgetExit]

			skip:
			jmp dword ptr [customCarBudgetSkip]
		}
	}



	constexpr address newCustomRequestEntrance = 0x43DF2A;
	constexpr address newCustomRequestExit     = 0x43DF8A;

	// Processes the new roadblock request for custom setups
	__declspec(naked) void NewCustomRequest()
	{
		static constexpr address newCustomRequestSkip = 0x43E1F3;

		__asm
		{
			fstp dword ptr [esp + 0x14]

			push dword ptr [esp + 0x14]  // roadWidth
			push dword ptr [esp + 0x4C8] // pursuit
			call UpdateAndAssessCustomRequest
			test al, al
			je skip                      // request unfeasible

			mov esi, dword ptr [maxNumCars]

			jmp dword ptr [newCustomRequestExit]

			skip:
			mov ecx, dword ptr [esp + 0x4C0]
			call CancelCustomRequest // ecx: caller

			jmp dword ptr [newCustomRequestSkip]
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool ExtractRoadblockParts
	(
		const auto& section,
		RBTable&    table
	) {
		bool hasSpikes = false;

		// Attempt parts extraction
		PartDataArray<RBPartType> partTypeIDs  = {};
		PartDataArray<float>      partOffsetsX = {};
		PartDataArray<float>      partOffsetsY = {};
		PartDataArray<float>      orientations = {};

		const PartDataArray<bool> isExtracteds = ConfigParser::Parser::ExtractArrays<maxNumParts, RBPartType, float, float, float>
		(
			section, {}, "part{:02}", HeatParameters::configFormatStart, {partTypeIDs}, {partOffsetsX}, {partOffsetsY}, {orientations}
		);

		// Process parts
		size_t numValidParts = 0;

		for (size_t partID = 0; partID < maxNumParts; ++partID)
		{
			if (not isExtracteds[partID]) continue; // invalid part

			switch (partTypeIDs[partID])
			{
			case RBPartType::CAR:
				++(table.numCarsRequired);
				break;

			case RBPartType::SAWHORSE:
				break;

			case RBPartType::SPIKES:
				hasSpikes = true;
				break;

			default:
				continue; // invalid part type
			}

			// Remove full rotation(s) and convert to positive value
			orientations[partID] -= std::floor(orientations[partID]);

			// Update part parameters
			table.parts[numValidParts] =
			{
				partTypeIDs [partID],
				partOffsetsX[partID],
				partOffsetsY[partID],
				orientations[partID]
			};

			++numValidParts;
		}

		return hasSpikes;
	}



	[[nodiscard]] RBTable CreateMirroredTable(const RBTable& table)
	{
		RBTable mirrored = table;

		for (RBPart& part : mirrored.parts)
		{
			if (part.type == RBPartType::NONE) break; // no more part(s)

			part.offsetX     = -part.offsetX;
			part.orientation = 1.f - part.orientation;

			if (part.type == RBPartType::SPIKES) // spike-strip pattern
				part.orientation = std::fmod(part.orientation + .5f, 1.f);
		}

		return mirrored;
	}



	[[nodiscard]] bool ExtractRoadblockSetup
	(
		const auto& section,
		RBSetup&    setup
	) {
		RBTable& table = setup.original;

		// Extract road widths
		if (not ConfigParser::Parser::ExtractScalars<float, float>(section, "extent", {table.minRoadWidth, {.001f}}, {setup.maxRoadWidth, {0.f}}))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(no extent)");

			return false; // missing road width(s)
		}

		if (table.minRoadWidth >= setup.maxRoadWidth)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(invalid extent)");

			return false; // invalid road width(s)
		}

		// Extract roadblock parts
		setup.hasSpikes = ExtractRoadblockParts(section, table);

		if (table.numCarsRequired == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(no car(s))");

			return false; // invalid part(s)
		}

		// Extract spawn parameters
		HeatParameters::Extract(section, setup.chance);

		if (setup.chance.GetMaximum() < 1)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail('-', setup.name, "(unused)");

			return false; // unused setup
		}

		ConfigParser::Parser::ExtractScalars<bool> (section, "stretch", {setup.canStretch});
		ConfigParser::Parser::ExtractScalars<float>(section, "mirror",  {setup.mirrorChance, {0.f, 100.f}});

		// Create mirrored roadblock table
		setup.mirrored = CreateMirroredTable(table);

		return true;
	}



	[[nodiscard]] bool ExtractRoadblockSetups(const ConfigParser::Parser& parser)
	{
		constexpr std::string_view setupPrefix = "Setups:";

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Roadblock setups:");

		const auto& nameToSection = parser.GetSectionMap();

		// Check (potential) setup count
		size_t maxNumSetups = 0;

		for (const auto& [name, section] : nameToSection)
			maxNumSetups += name.starts_with(setupPrefix);

		if (maxNumSetups == 0)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail("no setup(s) provided");

			return false; // no setups; disable feature
		}
		
		if constexpr (Globals::loggingEnabled)
			Globals::LogDetail(Globals::LogDec(maxNumSetups), "setup(s) provided");

		// Extract roadblock setups
		roadblockSetups.reserve(maxNumSetups);

		for (const auto& [name, section] : nameToSection)
		{
			if (not name.starts_with(setupPrefix)) continue; // not setup

			RBSetup setup(name.substr(setupPrefix.size()));
			if (not ExtractRoadblockSetup(section, setup)) continue; // invalid setup

			roadblockSetups.push_back(std::move(setup));
		}

		// Log and shrink setup vector
		if constexpr (Globals::loggingEnabled)
		{
			if (not roadblockSetups.empty())
			{
				Globals::LogDetail(Globals::LogDec(roadblockSetups.size()), "setup(s) valid");

				const auto [numRegular, numSpikes, numMirrorRegular, numMirrorSpikes] = CountAvailableSetups();

				Globals::LogDetail(Globals::LogDec(numRegular), "regular,", Globals::LogDec(numMirrorRegular), "mirrored");
				Globals::LogDetail(Globals::LogDec(numSpikes),  "spikes, ", Globals::LogDec(numMirrorSpikes),  "mirrored");
			}
			else Globals::LogDetail("no setup(s) valid");
		}

		roadblockSetups.shrink_to_fit();

		return (not roadblockSetups.empty());
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

		// Heat parameters
		HeatParameters::Extract(parser, "Roadblocks:Recycling", mayRecycleDistantCops);

		HeatParameters::Extract(parser, "Roadblocks:Radio", spawnCalloutChance, spikeCalloutChance);

		// Roadblock setups
		if (ExtractRoadblockSetups(parser))
		{
			// Code changes (conditional)
			MemoryTools::Write<float*>(&maxStretchScale, {0x43E334});

			MemoryTools::MakeRangeJMP<0x4063D0, 0x40644A>(SelectRoadblockTable); // replaces game function

			MemoryTools::MakeRangeJMP<customScaleEntrance,      customScaleExit>     (CustomScale);
			MemoryTools::MakeRangeJMP<customCarBudgetEntrance,  customCarBudgetExit> (CustomCarBudget);
			MemoryTools::MakeRangeJMP<newCustomRequestEntrance, newCustomRequestExit>(NewCustomRequest);
		}

		// Code Changes (general)
		MemoryTools::MakeRangeNOP<0x71F184, 0x71F19F>(); // regular callout
		MemoryTools::MakeRangeNOP<0x71F091, 0x71F096>(); // spikes  callout

		MemoryTools::MakeRangeJMP<spikeLaneEntrance,      spikeLaneExit>     (SpikeLane);
		MemoryTools::MakeRangeJMP<spikeCheckEntrance,     spikeCheckExit>    (SpikeCheck);
		MemoryTools::MakeRangeJMP<copRecyclingEntrance,   copRecyclingExit>  (CopRecycling);
		MemoryTools::MakeRangeJMP<requestOutcomeEntrance, requestOutcomeExit>(RequestOutcome);

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
		mayRecycleDistantCops.SetToHeatState(state);

		spawnCalloutChance.SetToHeatState(state);
		spikeCalloutChance.SetToHeatState(state);

		// Roadblock setups
		for (RBSetup& setup : roadblockSetups)
			setup.SetToHeatState(state);

		if constexpr (Globals::loggingEnabled)
		{
			if (roadblockSetups.empty()) return;

			const auto [numRegular, numSpikes, numMirrorRegular, numMirrorSpikes] = CountAvailableSetups();

			Globals::LogPlain("numRegularRoadblocks    ", Globals::LogDec(numRegular), '/', Globals::LogDec(numMirrorRegular));
			Globals::LogPlain("numSpikeRoadblocks      ", Globals::LogDec(numSpikes),  '/', Globals::LogDec(numMirrorSpikes));
		}
	}
}