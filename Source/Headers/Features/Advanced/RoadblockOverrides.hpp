#pragma once

#include <cmath>
#include <array>
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
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[RBL]";
	constexpr Globals::LogLiteral logName = "RoadblockOverrides";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, mayRecycleDistantCops, true);

	constinit HEAT_PARAMETER_VALUE(float, spawnCalloutChance, 100.f, {0.f, 100.f}); // percent
	constinit HEAT_PARAMETER_VALUE(float, spikeCalloutChance, 50.f,  {0.f, 100.f}); // percent

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(float, chaseRBJoinTimer,  {0.f}); // seconds
	constinit OPTIONAL_HEAT_PARAMETER_VALUE(float, backupRBJoinTimer, {0.f}); // seconds

	constinit HEAT_PARAMETER_VALUE(bool, reactToCooldownMode, true);
	constinit HEAT_PARAMETER_VALUE(bool, reactToSpikesHit,    true);

	constinit HEAT_PARAMETER_VALUE(float, maxRBJoinDistance,       500.f, {0.f}); // metres
	constinit HEAT_PARAMETER_VALUE(float, maxRBJoinElevationDelta, 1.5f,  {0.f}); // metres

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(int, maxJoinCountPerRB, {0}); // cars





	// Game types -----------------------------------------------------------------------------------------------------------------------------------

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

	template <typename T>
	using PartArray = std::array<T, maxNumParts>;



	struct RBTable
	{
	// Members

		float  minRoadWidth    = 0.f; // metres
		size_t numCarsRequired = 0;   // cars

		PartArray<RBPart> parts; // fully ASM-compatible
	};

	static_assert(sizeof(RBTable) == 104, "Table-size mismatch");





	// RBSetup class --------------------------------------------------------------------------------------------------------------------------------

	class RBSetup
	{
	private: // friends

		friend bool ExtractRoadblockSetup(const auto&, RBSetup&); // forward declarations... grrr!


	private: // members

		bool hasSpikes = false;

		float maxRoadWidth = 0.f; // metres

		float mirrorChance = 0.f;  // percent
		float stretchLimit = 14.f; // percent

		RBTable original; // same constraints as mirrored
		RBTable mirrored; // same constraints as original

		HEAT_PARAMETER_VALUE(int, chance, 100, {0}); // relative

		[[no_unique_address]] Globals::LogString name;

	
	public: // methods

		explicit RBSetup(const std::string_view name) : name(name) {}


		void SetToHeatState(const HeatParameters::HeatState state)
		{
			this->chance.SetToHeatStateQuietly(state);
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


		[[nodiscard]] bool DoMirrorTrial() const
		{
			return Globals::pRNG.DoPercentTrial<float>(this->mirrorChance);
		}


		[[nodiscard]] const RBTable& GetTable(const bool mirrored) const
		{
			return (mirrored) ? this->mirrored : this->original;
		}


		[[nodiscard]] float GetMaxStretchScale() const
		{
			return 1.f + (this->stretchLimit / 100.f);
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





	// Feature setup (continued) --------------------------------------------------------------------------------------------------------------------

	// Custom roadblock setups
	RELEASE_CONSTINIT std::vector<RBSetup> roadblockSetups;

	// Assembly detours
	size_t numCarsToSource = 0;     // used only if setups non-empty
	float  maxStretchScale = 1.14f; // used only if setups non-empty

	bool hasSpikes = false;
	int  spikeLane = 0;

	



	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __cdecl ReportSelectionAttempt
	(
		const address pursuit,
		const float   roadWidth,
		const size_t  maxNumCars,
		const bool    needsSpikes
	) {
		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogFull(pursuit, logTag, "Roadblock selection");

			if (roadblockSetups.empty())
			{
				Globals::LogPlain("Has spikes:", needsSpikes);
				Globals::LogPlain("Road width:", roadWidth);
			}

			Globals::LogPlain("Car budget:", Globals::LogDec(maxNumCars));
		}
	}



	[[nodiscard]] float __stdcall ApplyStretchScale(const float widthRatio)
	{
		const float clampedRatio = (maxStretchScale > 1.f) ? std::clamp<float>(widthRatio, 1.f, maxStretchScale) : 1.f;

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain(widthRatio, "->", clampedRatio);

		return clampedRatio;
	}



	[[nodiscard]] float GetRoadblockSpikeChance(const address pursuit)
	{
		const float* const spikeChance = AsPointer<float>(Globals::GetFromPursuitLevels(pursuit, "roadblockspikechance"_vlt));

		if (not spikeChance)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Invalid roadblockspikechance pointer in", pursuit);

			ASSERT_UNREACHABLE_THEN(return 0.f);
		}

		return *spikeChance;
	}



	[[nodiscard]] bool IsRequestFeasible
	(
		const address pursuit,
		const bool    anyRegular,
		const bool    anySpike
	) {
		if (anyRegular and anySpike)      return true;  // both    available
		if (not (anyRegular or anySpike)) return false; // neither available

		if (Globals::Pursuit::IsSearching(pursuit)) return anyRegular;

		const float spikeChance = GetRoadblockSpikeChance(pursuit);

		if (spikeChance <= 0.f)   return anyRegular; // always regular
		if (spikeChance >= 100.f) return anySpike;   // always spike

		return true; // either available
	}



	[[nodiscard]] bool ShouldRequireSpikes
	(
		const address pursuit,
		const bool    anyRegular,
		const bool    anySpike
	) {
		if (not (anyRegular or anySpike))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Incompatible request in", pursuit);

			ASSERT_UNREACHABLE_THEN(return false); // will fail anyway
		}

		if (Globals::Pursuit::IsSearching(pursuit)) return false;

		const float spikeChance = GetRoadblockSpikeChance(pursuit);

		if (spikeChance <= 0.f)   return false; // never spikes
		if (spikeChance >= 100.f) return true;  // always spikes

		if (not anyRegular) return true;  // regular impossible
		if (not anySpike)   return false; // spikes  impossible

		return Globals::pRNG.DoPercentTrial<float>(spikeChance); // either possible
	}



	[[nodiscard]] bool __stdcall AssessRoadblockRequest
	(
		const address pursuit, 
		const float   roadWidth
	) {
		struct SetupReport
		{
			size_t numSetups  = 0;
			size_t maxNumCars = 0;
		};

		SetupReport regular;
		SetupReport spike;

		for (const RBSetup& setup : roadblockSetups)
		{
			if (not setup.IsAvailable())                   continue;
			if (not setup.IsCompatbleRoadWidth(roadWidth)) continue;

			auto& report = (setup.HasSpikes()) ? spike : regular;

			report.maxNumCars = std::max<size_t>(report.maxNumCars, setup.GetNumCarsRequired());

			++(report.numSetups);
		}

		const bool anyRegular = (regular.numSetups > 0);
		const bool anySpike   = (spike  .numSetups > 0);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogFull(pursuit, logTag, "Roadblock attempt");

			Globals::LogPlain("Road width:", roadWidth);
			Globals::LogPlain("Candidates:", Globals::LogDec(regular.numSetups), '/', Globals::LogDec(spike.numSetups));
		}

		if (not IsRequestFeasible(pursuit, anyRegular, anySpike)) return false; // cancel request

		hasSpikes       = ShouldRequireSpikes(pursuit, anyRegular, anySpike);
		numCarsToSource = (hasSpikes) ? spike.maxNumCars : regular.maxNumCars;

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Has spikes:", hasSpikes);

		return true; // continue request
	}



	void __fastcall CancelRoadblockRequest
	(
		const address pursuit,
		const address caller
	) {
		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Cancelling request");

		switch (caller)
		{
		case 0x43E7D6: // HeavyStrategy 4
			Globals::ClearSupportRequest(pursuit);
			return;

		case 0x43EC3A: // non-Strategy roadblock
			AsReference<bool>   (pursuit             + 0x190) = false; // request status
			AsReference<address>(Globals::copManager + 0xBC)  = 0x0;   // roadblock pursuit
			AsReference<int>    (Globals::copManager + 0xB8)  = 0;     // car count
			return;
		}

		if constexpr (Globals::loggingEnabled)
			Globals::LogWarning(logTag, "Unknown CreateRoadblock caller:", caller);

		ASSERT_UNREACHABLE;
	}



	void __fastcall CallOutRoadblockSpawn(const address pursuit)
	{
		if (Globals::Pursuit::IsSearching(pursuit)) return;
		if (not Globals::IsPlayerPursuit(pursuit))  return;

		if (not Globals::pRNG.DoPercentTrial<float>(spawnCalloutChance.current))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "No callout");

			return; // skip callout
		}

		if (hasSpikes and Globals::pRNG.DoPercentTrial<float>(spikeCalloutChance.current))
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



	[[nodiscard]] bool __fastcall IsJoinCountExhausted(const address pursuit)
	{
		if (not maxJoinCountPerRB.isEnabled.current) return false;

		const int numVehiclesJoined = AsReference<int>(pursuit + 0x23C);
		return (numVehiclesJoined >= maxJoinCountPerRB.value.current);
	}



	[[nodiscard]] bool __fastcall HasJoinTimerExpired(const address roadblock)
	{
		const address pursuit       = AsReference<address>(roadblock + 0x28);
		const int     pursuitStatus = AsReference<int>    (pursuit   + 0x218);

		const auto HasExpired = [roadblock](const auto& joinTimer) -> bool
		{
			if (not joinTimer.isEnabled.current) return false;

			const float timeInProximity = AsReference<float>(roadblock + 0x58);
			return (timeInProximity >= joinTimer.value.current);
		};

		switch (pursuitStatus)
		{
		case 0: // default pursuit state
			return HasExpired(chaseRBJoinTimer);

		case 1: // active "Backup" timer
			return HasExpired(backupRBJoinTimer);

		case 2: // "COOLDOWN" mode
			return reactToCooldownMode.current;
		}

		return false;
	}





	// Replacement functions ------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const RBTable* __cdecl SelectRoadblockTable
	(
		const float  roadWidth,
		const size_t maxNumCars,
		const bool   needsSpikes
	) {
		static RELEASE_CONSTINIT std::vector<const RBSetup*> candidates;

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
				Globals::LogWarning(logTag, "No candidate(s)");

			ASSERT_UNREACHABLE_THEN(return nullptr);
		}

		// Select a random eligible setup
		int       cumulativeChance = 0;
		const int chanceThreshold  = Globals::pRNG.GenerateNumber<int>(1, totalChance);

		for (const RBSetup* const setup : candidates)
		{
			cumulativeChance += setup->GetChance();
			if (cumulativeChance < chanceThreshold) continue;

			const bool mirrored = setup->DoMirrorTrial();
			maxStretchScale     = setup->GetMaxStretchScale();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain(setup->GetName(), (mirrored) ? "(mirrored)" : "(original)");

			const auto* const table = &(setup->GetTable(mirrored));

			candidates.clear(); // safe due to immediate return

			return table;
		}

		candidates.clear();

		if constexpr (Globals::loggingEnabled)
			Globals::LogWarning(logTag, "Failed to select setup");

		ASSERT_UNREACHABLE_THEN(return nullptr);
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Checks the timer for joining from roadblocks
	ASSEMBLY_DETOUR(JoinTimer, /* begin = */ 0x42BF06, /* end = */ 0x42BF2B)
	{
		__asm
		{
			fstp dword ptr [ebp + 0x58] // join timer

			mov ecx, ebp
			call HasJoinTimerExpired // ecx: roadblock
			cmp al, 1

			EXIT_ASSEMBLY_DETOUR(JoinTimer)
		}
	}



	// Enforces the per-roadblock join count
	ASSEMBLY_DETOUR(JoinCount, 0x444397, 0x4443AC)
	{
		__asm
		{
			mov ecx, dword ptr [esi + 0xC4]

			cmp byte ptr [ecx + 0x5C], 1 // join-status flag
			jne conclusion               // not ready to join

			lea ecx, dword ptr [esi + 0x40]
			call IsJoinCountExhausted // ecx: pursuit
			test al, al

			conclusion:
			EXIT_ASSEMBLY_DETOUR(JoinCount)
		}
	}



	// Records the last spike-strip position in the current roadblock
	ASSEMBLY_DETOUR(SpikeLane, 0x43E574, 0x43E57B)
	{
		__asm
		{
			mov dword ptr [spikeLane], eax

			// Execute original code and resume
			lea ecx, dword ptr [esp + 0xA4]

			EXIT_ASSEMBLY_DETOUR(SpikeLane)
		}
	}



	// Pre-processes incoming roadblock requests
	ASSEMBLY_DETOUR(NewRequest, 0x43DF2A, 0x43DF8A)
	{
		static constexpr address cancellationExit = 0x43E1F3;

		__asm
		{
			fstp dword ptr [esp + 0x14]

			push dword ptr [esp + 0x14]  // roadWidth
			push dword ptr [esp + 0x4C8] // pursuit
			call AssessRoadblockRequest
			test al, al
			je cancellation              // request unfeasible

			mov esi, dword ptr [numCarsToSource]

			EXIT_ASSEMBLY_DETOUR(NewRequest)

			cancellation:
			mov ecx, dword ptr [esp + 0x4C4]
			mov edx, dword ptr [esp + 0x4C0]
			call CancelRoadblockRequest // ecx: pursuit; edx: caller

			jmp dword ptr [cancellationExit]
		}
	}



	// Applies the maximum stretch-scale for roadblocks
	ASSEMBLY_DETOUR(StretchScale, 0x43E31D, 0x43E359)
	{
		__asm
		{
			push ecx

			sub esp, 0x4

			fstp dword ptr [esp] // widthRatio
			call ApplyStretchScale
			fstp dword ptr [esp + 0x30]

			pop ecx

			EXIT_ASSEMBLY_DETOUR(StretchScale)
		}
	}



	// Decides whether to recycle cops for the roadblock
	ASSEMBLY_DETOUR(CopRecycling, 0x43E0EF, 0x43E0F5)
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
			EXIT_ASSEMBLY_DETOUR(CopRecycling)
		}
	}



	// Records whether vanilla roadblocks have spikes
	ASSEMBLY_DETOUR(VanillaSpike, 0x43E1DA, 0x43E1E0)
	{
		__asm
		{
			mov al, byte ptr [esp + 0x10]
			mov byte ptr [hasSpikes], al

			// Execute original code and resume
			mov dword ptr [esp + 0x18], ecx
			cmp ecx, edi

			EXIT_ASSEMBLY_DETOUR(VanillaSpike)
		}
	}


	
	// Checks the amount of sourced vehicles for roadblocks
	ASSEMBLY_DETOUR(CarBudgetCheck, 0x43E146, 0x43E1C5)
	{
		static constexpr address failureExit = 0x43E1E2;

		__asm
		{
			cmp ebp, dword ptr [numCarsToSource]
			jl failure // insufficient car budget

			mov al, byte ptr [hasSpikes]
			mov byte ptr [esp + 0x10], al

			// Execute original code and resume
			mov esi, dword ptr [esp + 0x4C4]
			xor edi, edi

			EXIT_ASSEMBLY_DETOUR(CarBudgetCheck)

			failure:
			jmp dword ptr [failureExit]
		}
	}


	
	// Prepares arguments for roadblock-selection function
	ASSEMBLY_DETOUR(SelectionAttempt, 0x43E1C5, 0x43E1D0)
	{
		__asm
		{
			// Execute original code first
			movzx eax, byte ptr [esp + 0x10]
			mov ecx, dword ptr [esp + 0x14]

			push eax // needsSpikes
			push ebp // maxNumCars
			push ecx // roadWidth

			push dword ptr [esp + 0x4D0] // pursuit
			call ReportSelectionAttempt
			add esp, 0x4

			EXIT_ASSEMBLY_DETOUR(SelectionAttempt)
		}
	}



	// Processes the outcome of the roadblock request
	ASSEMBLY_DETOUR(RoadblockOutcome, 0x43E20C, 0x43E213)
	{
		__asm
		{
			test al, al
			je conclusion // request failed

			mov ecx, dword ptr [esp + 0x4C4]
			call CallOutRoadblockSpawn // ecx: pursuit

			mov al, 1 // restore value

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x4B4]

			EXIT_ASSEMBLY_DETOUR(RoadblockOutcome)
		}
	}



	// Can suppress roadblock reactions to spike-strip hits
	ASSEMBLY_DETOUR(SpikesHitReaction, 0x63BB9A, 0x63BBA6)
	{
		static constexpr float maxJoinRange = 80.f; // metres

		__asm
		{
			// Execute original code first
			mov eax, dword ptr [eax + 0x70] // roadblock pursuit
			test eax, eax
			je conclusion                   // no pursuit

			cmp byte ptr [reactToSpikesHit.current], 0
			je conclusion // reaction disabled

			mov edx, eax

			fld dword ptr [edx + 0x7C] // distance to target
			fcomp dword ptr [maxJoinRange]
			fnstsw ax
			test ah, 0x41

			mov eax, edx

			conclusion:
			EXIT_ASSEMBLY_DETOUR(SpikesHitReaction)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	bool ExtractRoadblockParts
	(
		const auto& section,
		RBTable&    table
	) {
		bool hasSpikes = false;

		// Attempt parts extraction
		PartArray<RBPartType> types        = {};
		PartArray<float>      offsetXs     = {};
		PartArray<float>      offsetYs     = {};
		PartArray<float>      orientations = {};

		const PartArray<bool> isExtracteds = ConfigParser::Parser::ExtractArrays<maxNumParts, RBPartType, float, float, float>
		(
			section, /* defaultKey = */ {}, "part{:02}", /* keyStartIndex = */ 1, {types}, {offsetXs}, {offsetYs}, {orientations}
		);

		// Process parts
		size_t numValidParts = 0;

		for (size_t partID = 0; partID < maxNumParts; ++partID)
		{
			if (not isExtracteds[partID]) continue; // invalid part

			switch (types[partID])
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
				.type        = types       [partID],
				.offsetX     = offsetXs    [partID],
				.offsetY     = offsetYs    [partID],
				.orientation = orientations[partID]
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



	bool ExtractRoadblockSetup
	(
		const auto& section,
		RBSetup&    setup
	) {
		RBTable& table = setup.original;

		// Extract road widths
		const bool widthsExtracted = ConfigParser::Parser::ExtractScalars<float, float>
		(
			section, "extent", {table.minRoadWidth, {.001f}}, {setup.maxRoadWidth, {.001f}}
		);

		if (not widthsExtracted)
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

		ConfigParser::Parser::ExtractScalars<float>(section, "mirror",  {setup.mirrorChance, {0.f, 100.f}});
		ConfigParser::Parser::ExtractScalars<float>(section, "stretch", {setup.stretchLimit, {0.f, 20.f}});

		// Create mirrored roadblock table
		setup.mirrored = CreateMirroredTable(table);

		return true;
	}



	bool ExtractRoadblockSetups(const ConfigParser::Parser& parser)
	{
		constexpr std::string_view setupPrefix = "Setups:";

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Roadblock setups:");

		const auto& nameToSection = parser.GetSectionMap();

		// Check (potential) setup count
		size_t maxNumSetups = 0;

		for (const auto& [name, _] : nameToSection)
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

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::fileRoadblocks);

		// Heat parameters
		HeatParameters::Extract(parser, "Roadblocks:Recycling", mayRecycleDistantCops);

		HeatParameters::Extract(parser, "Roadblocks:Radio", spawnCalloutChance, spikeCalloutChance);

		HeatParameters::Extract(parser, "Roadblocks:Joining", chaseRBJoinTimer, backupRBJoinTimer);

		HeatParameters::Extract(parser, "Joining:Reactions", reactToCooldownMode, reactToSpikesHit);

		HeatParameters::Extract(parser, "Joining:Proximity", maxRBJoinDistance, maxRBJoinElevationDelta);

		HeatParameters::Extract(parser, "Joining:Count", maxJoinCountPerRB);

		// Roadblock setups
		if (ExtractRoadblockSetups(parser))
		{
			// Code modifications (conditional)
			MemoryTools::Write<size_t>(maxNumParts, {0x40AFD9}); // spikes reaction

			MemoryTools::ReplaceCall(0x43E1D0, SelectRoadblockTable); // PickRoadblockSetup (0x4063D0)

			PATCH_ASSEMBLY_DETOUR(NewRequest);
			PATCH_ASSEMBLY_DETOUR(StretchScale);
			PATCH_ASSEMBLY_DETOUR(CarBudgetCheck);
		}
		else PATCH_ASSEMBLY_DETOUR(VanillaSpike); // only for vanilla roadblocks

		// Code modifications (general)
		MemoryTools::Write<float*>(&(maxRBJoinDistance      .current), {0x42BEBC});
		MemoryTools::Write<float*>(&(maxRBJoinElevationDelta.current), {0x42BE3A});

		MemoryTools::MakeRangeNOP<0x71F184, 0x71F19F>(); // regular callout
		MemoryTools::MakeRangeNOP<0x71F091, 0x71F096>(); // spikes  callout

		PATCH_ASSEMBLY_DETOUR(JoinTimer);
		PATCH_ASSEMBLY_DETOUR(JoinCount);
		PATCH_ASSEMBLY_DETOUR(SpikeLane);
		PATCH_ASSEMBLY_DETOUR(CopRecycling);
		PATCH_ASSEMBLY_DETOUR(RoadblockOutcome);
		PATCH_ASSEMBLY_DETOUR(SpikesHitReaction);

		// Code modifications (logging)
		if constexpr (Globals::loggingEnabled)
			PATCH_ASSEMBLY_DETOUR(SelectionAttempt);

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

		chaseRBJoinTimer .SetToHeatState(state);
		backupRBJoinTimer.SetToHeatState(state);

		reactToCooldownMode.SetToHeatState(state);
		reactToSpikesHit   .SetToHeatState(state);

		maxRBJoinDistance      .SetToHeatState(state);
		maxRBJoinElevationDelta.SetToHeatState(state);

		maxJoinCountPerRB.SetToHeatState(state);

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