#pragma once

#include <span>
#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "../../Features/Basic/GroundSupport.hpp"
#include "../../Features/Basic/GeneralSettings.hpp"

#include "CopSpawnTables.hpp"
#include "PursuitFeatures.hpp"



namespace CopSpawnOverrides
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[SPA]";
	constexpr Globals::LogLiteral logName = "CopSpawnOverrides";

	// Pursuit-board tracking
	bool trackHeavyVehicles     = false;
	bool trackLeaderVehicles    = false;
	bool trackRoadblockVehicles = false;

	// Heat parameters
	constinit HEAT_PARAMETER_INTERVAL(int, activeChaserLimit, 1, 8, {0}); // cars

	constinit HEAT_PARAMETER_VALUE(bool, chasersAreIndependent, false);

	constinit HEAT_PARAMETER_VALUE(bool, onlyDestroyedDecrement, false);

	constinit HEAT_PARAMETER_VALUE(bool, transitionTriggersBackup, false);

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, chaserSpawnDistance, {150.f, 400.f});

	constinit HEAT_PARAMETER_VALUE(float, chaserSpawnClearance, 40.f, {0.f}); // metres

	constinit HEAT_PARAMETER_VALUE(bool, trafficIgnoresChasers,    false);
	constinit HEAT_PARAMETER_VALUE(bool, trafficIgnoresRoadblocks, false);

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(int, roadblockJoinLimit, {0}); // cars

	// Parameter conversions
	float squaredChaserSpawnClearance; // metres squared





	// Contingent class -----------------------------------------------------------------------------------------------------------------------------

	#define COP_CONTINGENT(name, ...) CopSpawnOverrides::Contingent name{#name, __VA_ARGS__}

	class Contingent
	{
	private: // aliases

		using TableObject = HeatParameters::Object<CopSpawnTables::SpawnTable>;


	private: // members

		int numTotalActiveCops = 0; // cars

		const address            pursuit; // pursuit-locked and immobile
		const TableObject* const source;  // reference would break constinit in MSVC
		
		mutable const char* cachedCopName = nullptr; // only cleared on spawn success

		CopSpawnTables::SpawnTable table;

		ModContainers::VaultMap<int> copTypeToNumActive; // cars

		[[no_unique_address]] Globals::LogLiteral name;


	private: // methods

		bool ChangeNumActive
		(
			const vault copType, 
			const int   change
		) {
			if (change == 0) return true;

			const auto [pairIt, _] = this->copTypeToNumActive.insert(copType, /* numActive = */ 0);

			int& numActiveCops = pairIt->second;

			numActiveCops            += change;
			this->numTotalActiveCops += change;

			this->table.ChangeNumActive(copType, change);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
				{
					Globals::LogPlain("Type ratio:", numActiveCops, '/', this->numTotalActiveCops);
					Globals::LogPlain(this->table.GetNumAvailable(copType), "more available");
				}
			}

			const bool hasMiscounted = (numActiveCops < 0);

			if (hasMiscounted)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, copType, "miscount in", this->name);

				ASSERT_UNREACHABLE;
			}
				
			if (numActiveCops < 1)
				this->copTypeToNumActive.erase(pairIt);

			return (not hasMiscounted);
		}


	public: // methods

		constexpr Contingent
		(
			const Globals::LogLiteral name, 
			const TableObject&        source
		) 
			: name(name), source(&source), pursuit(0x0), table()
		{
		}


		Contingent
		(
			const Globals::LogLiteral name,
			const TableObject&        source,
			const address             pursuit
		) 
			: name(name), source(&source), pursuit(pursuit), table(*(source.current))
		{
		}


		Contingent(Contingent&&)      = delete;
		Contingent(const Contingent&) = delete;

		Contingent& operator=(Contingent&&)      = delete;
		Contingent& operator=(const Contingent&) = delete;


		void Reserve(const size_t numTypes)
		{
			this->copTypeToNumActive.reserve(numTypes);
		}


		void UpdateSpawnTable()
		{
			this->cachedCopName = nullptr;

			const auto* const sourceTable = this->source->current;

			if (not sourceTable)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid source pointer in", this->name);

				ASSERT_UNREACHABLE_THEN(return);
			}

			this->table = *sourceTable;

			// Copy existing cops over to new table
			for (const auto& [copType, numActiveCops] : this->copTypeToNumActive)
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (this->pursuit)
						Globals::LogPlain("Copied", numActiveCops, Globals::VehicleType::GetName(copType));
				}

				this->table.ChangeNumActive(copType, numActiveCops);
			}
		}


		void __thiscall Clear()
		{
			this->numTotalActiveCops = 0;

			this->cachedCopName = nullptr;
			this->table.ResetNumActive();

			this->copTypeToNumActive.clear();
		}


		void IncrementByType(const vault copType)
		{
			this->cachedCopName = nullptr; // almost always matches copType

			this->ChangeNumActive(copType, /* change = */ +1);
		}


		void __thiscall Increment(const address copVehicle)
		{
			this->IncrementByType(Globals::GetVehicleType(copVehicle));
		}


		bool DecrementByType(const vault copType)
		{
			return this->ChangeNumActive(copType, /* change = */ -1);
		}


		bool __thiscall Decrement(const address copVehicle)
		{
			return this->DecrementByType(Globals::GetVehicleType(copVehicle));
		}


		[[nodiscard]] int GetNumTotalActiveCops() const
		{
			return this->numTotalActiveCops;
		}


		[[nodiscard]] bool IsAnyCopAvailable() const
		{
			return this->table.IsAnyCopAvailable();
		}


		[[nodiscard]] const char* GetNameOfAvailableCop() const
		{
			if (not this->cachedCopName)
				this->cachedCopName = this->table.GetNameOfAvailableCop();

			return this->cachedCopName;
		}


		[[nodiscard]] const char* GetNewNameOfAvailableCop() const
		{
			return this->table.GetNameOfAvailableCop();
		}


		[[nodiscard]] const char* __thiscall GetNameOfAvailableCopWithFallback() const
		{
			if (const auto nameFromTable = this->GetNameOfAvailableCop()) return nameFromTable;
				
			const auto* const sourceTable = this->source->current;

			if (not sourceTable)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid source-table pointer in", this->name);

				ASSERT_UNREACHABLE_THEN(return "copmidsize");
			}

			this->cachedCopName = sourceTable->GetNameOfAvailableCop();

			return this->cachedCopName;
		}


		[[nodiscard]] const char* GetNewNameOfAvailableCopWithFallback() const
		{
			if (const auto nameFromTable = this->GetNewNameOfAvailableCop()) return nameFromTable;

			const auto* const sourceTable = this->source->current;

			if (not sourceTable)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid source-table pointer in", this->name);

				ASSERT_UNREACHABLE_THEN(return "copmidsize");
			}

			return sourceTable->GetNameOfAvailableCop();
		}
	};





	// Feature setup (continued) --------------------------------------------------------------------------------------------------------------------

	// Inline hashes for ASM
	enum class VaultHash : vault
	{
		AIGOALPATROL = "AIGoalPatrol"_vlt
	};

	// Assembly detours
	RELEASE_CONSTINIT COP_CONTINGENT(sceneSpawns,     CopSpawnTables::chasersTable);
	RELEASE_CONSTINIT COP_CONTINGENT(patrolSpawns,    CopSpawnTables::patrolsTable);
	RELEASE_CONSTINIT COP_CONTINGENT(scriptedSpawns,  CopSpawnTables::scriptedTable);
	RELEASE_CONSTINIT COP_CONTINGENT(roadblockSpawns, CopSpawnTables::roadblockTable);

	bool        eventHasScriptedPursuit = false;   // scripted free-roam pursuits request a cop before they know their Heat level,
	bool        usePrefetchedCopName    = false;   // so we must prefetch a valid cop name using their event's Heat level instead
	const char* prefetchedCopName       = nullptr; // (this is completely unrelated to knowing the player vehicle's Heat level)





	// ChasersManager class -------------------------------------------------------------------------------------------------------------------------

	class ChasersManager : public PursuitFeatures::Reaction, public PursuitFeatures::Searchable<ChasersManager>
	{
	private: // members

		bool waveParametersKnown = false;

		int maxNumPatrolCars           = 0; // cars
		int numSupportVehicles         = 0; // cars
		int numTrackedNonChasers       = 0; // cars
		int numJoinedRoadblockVehicles = 0; // cars

		int& pursuitStatus = AsReference<int>(this->pursuit + 0x218);

		float& backupTimer = AsReference<float>(this->pursuit + 0x21C); // seconds

		int& fullWaveCapacity       = AsReference<int>(this->pursuit + 0x144); // cars
		int& numCopsLostInWave      = AsReference<int>(this->pursuit + 0x14C); // cars
		int& numCopsToTriggerBackup = AsReference<int>(this->pursuit + 0x148); // cars

		const address& roadblock = AsReference<address>(this->pursuit + 0x84);

		const bool& isPerpBusted      = AsReference<bool>(this->pursuit + 0xE8);
		const bool& bailingPursuit    = AsReference<bool>(this->pursuit + 0xE9);
		const bool& isFreeRoamPursuit = AsReference<bool>(this->pursuit + 0xA8);

		const float& copSpawnCooldown  = AsReference<float>(this->pursuit + 0xCC); // seconds

		COP_CONTINGENT(chaserSpawns, CopSpawnTables::chasersTable, this->pursuit);

		inline static constexpr Globals::LogLiteral name = "ChasersManager";


	private: // methods

		void UpdateSpawnTable()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Updating table");

			this->chaserSpawns.UpdateSpawnTable();
		}


		void UpdateNumPatrolCars()
		{
			const int* const numPatrolCars = AsPointer<int>(Globals::GetFromPursuitLevels(this->pursuit, "NumPatrolCars"_vlt));

			if (not numPatrolCars)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid NumPatrolCars pointer in", this->pursuit);

				ASSERT_UNREACHABLE;
			} 

			this->maxNumPatrolCars = (numPatrolCars) ? *numPatrolCars : 1;

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Max. patrol cars:", this->maxNumPatrolCars);
		}


		[[nodiscard]] int GetWaveCapacity() const
		{
			const int numActiveChasers = this->chaserSpawns.GetNumTotalActiveCops();
			int       waveCapacity     = this->fullWaveCapacity - (this->numCopsLostInWave + numActiveChasers);

			if (this->waveParametersKnown)
				waveCapacity -= this->numTrackedNonChasers;

			return waveCapacity;
		}


		void CorrectWaveCapacity() const
		{
			if (not this->waveParametersKnown) return;

			const int waveCapacity = this->GetWaveCapacity();

			if (waveCapacity < 0)
			{
				this->fullWaveCapacity -= waveCapacity;

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Wave correction:", -waveCapacity);
			}
		}


		[[nodiscard]] int GetNumRoadblockVehicles() const
		{
			if (not this->roadblock) return 0;

			const address firstVehicleEntry = AsReference<address>(this->roadblock + 0xC);
			const address lastVehicleEntry  = AsReference<address>(this->roadblock + 0x10);

			if (lastVehicleEntry < firstVehicleEntry)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid roadblock data in", this->pursuit);

				ASSERT_UNREACHABLE_THEN(return 0);
			}

			return (lastVehicleEntry - firstVehicleEntry) / sizeof(address);
		}


		[[nodiscard]] static int GetGlobalNumNonRoadblockVehicles()
		{
			int numNonRoadblockVehicles = AsReference<int>(Globals::copManager + 0x94); // cops loaded

			for (const auto* const instance : ChasersManager::GetInstances())
				numNonRoadblockVehicles -= instance->GetNumRoadblockVehicles();

			if (numNonRoadblockVehicles < 0)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid vehicle count:", numNonRoadblockVehicles);

				ASSERT_UNREACHABLE_THEN(return 0);
			}

			return numNonRoadblockVehicles;
		}


		[[nodiscard]] bool MayNewChaserSpawn() const
		{
			if (this->isPerpBusted)           return false;
			if (this->bailingPursuit)         return false;
			if (this->copSpawnCooldown > 0.f) return false;

			if (not this->chaserSpawns.IsAnyCopAvailable()) return false;

			const int numActiveChasers  = this->chaserSpawns.GetNumTotalActiveCops();
			const int numActiveVehicles = (chasersAreIndependent.current) ? numActiveChasers : this->GetGlobalNumNonRoadblockVehicles();

			if (numActiveVehicles >= activeChaserLimit.max.current) return false;
			if (Globals::Pursuit::IsSearching(this->pursuit))       return (numActiveChasers < this->maxNumPatrolCars);

			return ((numActiveChasers < activeChaserLimit.min.current) or (this->GetWaveCapacity() > 0));
		}


		[[nodiscard]] bool IsBackUpTimerActive() const
		{
			return (this->pursuitStatus == 1);
		}


		void ForceTriggerBackup() const
		{
			const auto LockInPursuitAttributes = AsFunction<void __thiscall (address)>(0x40A9B0);

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Force-triggering backup");

			if (this->IsBackUpTimerActive())
			{
				this->backupTimer   = 0.f;
				this->pursuitStatus = 0;
			}

			LockInPursuitAttributes(this->pursuit);
		}


		[[nodiscard]] static bool HasVehicleEngaged(const address copVehicle)
		{
			const address copAIVehiclePursuit = Globals::Vehicle::GetAIVehiclePursuit(copVehicle);
			ASSERT_CONDITION_THEN_IF_FALSE(copAIVehiclePursuit, return false);

			return AsReference<bool>(copAIVehiclePursuit + 0x22);
		}


		void ProcessAddedChaser(const address copVehicle)
		{
			this->chaserSpawns.Increment(copVehicle);

			this->CorrectWaveCapacity();
		}


		[[nodiscard]] static bool IsTrackedNonChaser(const CopLabel copLabel)
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				return trackHeavyVehicles;

			case CopLabel::LEADER:
				return trackLeaderVehicles;

			case CopLabel::ROADBLOCK:
				return trackRoadblockVehicles;
			}

			return false;
		}


		void ProcessNonChaserChange
		(
			const CopLabel copLabel,
			const int      change
		) {
			// Vehicle counts
			switch (copLabel)
			{
			case CopLabel::HEAVY:
			case CopLabel::LEADER:
				this->numSupportVehicles += change;

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Support vehicles:", this->numSupportVehicles);

				break;

			case CopLabel::ROADBLOCK:
				this->numJoinedRoadblockVehicles += change;

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Roadblock vehicles:", this->numJoinedRoadblockVehicles);
			}

			// Non-Chaser tracking
			if (not this->IsTrackedNonChaser(copLabel)) return;

			this->numTrackedNonChasers += change;

			if (this->waveParametersKnown)
			{
				this->fullWaveCapacity       += change;
				this->numCopsToTriggerBackup += change;
			}
		}


		void ProcessRemovedChaser(const address copVehicle)
		{
			if (not this->chaserSpawns.Decrement(copVehicle))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Unknown chaser", copVehicle, "in", this->pursuit);

				ASSERT_UNREACHABLE_THEN(return);
			}

			if (not (this->isFreeRoamPursuit or (GeneralSettings::anyFeatureEnabled and GeneralSettings::trackCopsLost)))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "No decrement (tracking)");

				return; // chaser not tracked
			}

			if (onlyDestroyedDecrement.current and (not Globals::IsVehicleDestroyed(copVehicle)))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "No decrement (wrecking)");

				return; // chaser not wrecked
			}

			if (not this->HasVehicleEngaged(copVehicle))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "No decrement (engagement)");

				return; // chaser not engaged
			}

			++(this->numCopsLostInWave);
		}


	public: // members

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


	public: // methods

		explicit ChasersManager(const address pursuit) : Reaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('+', this, this->name);

			this->chaserSpawns.Reserve(20);
		}


		~ChasersManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('-', this, this->name);
		}


		void ReactToHeatStateUpdate() override 
		{
			this->UpdateSpawnTable();
		}


		void ReactToHeatStateUpdateWithDelay() override
		{
			this->UpdateNumPatrolCars();

			if (transitionTriggersBackup.current)
				this->ForceTriggerBackup();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::CHASER)
				this->ProcessAddedChaser(copVehicle);

			else this->ProcessNonChaserChange(copLabel, /* change = */ +1);
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::CHASER)
				this->ProcessRemovedChaser(copVehicle);

			else this->ProcessNonChaserChange(copLabel, /* change = */ -1);
		}


		static void __fastcall NotifyOfWaveReset(const address pursuit)
		{
			auto* const manager = ChasersManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			if constexpr (Globals::loggingEnabled)
			{
				if (not manager->waveParametersKnown)
					Globals::LogFull(manager->pursuit, logTag, "Wave parameters now known");
			}

			manager->waveParametersKnown = true;

			manager->fullWaveCapacity       += manager->numTrackedNonChasers;
			manager->numCopsToTriggerBackup += manager->numTrackedNonChasers;

			manager->CorrectWaveCapacity();
		}


		[[nodiscard]] static bool __fastcall IsChaserAvailable(const address pursuit)
		{
			const auto* const manager = ChasersManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return false);

			return manager->MayNewChaserSpawn();
		}


		[[nodiscard]] static bool __fastcall MayRoadblockVehicleJoin(const address pursuit)
		{
			const auto* const manager = ChasersManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return false);

			if (roadblockJoinLimit.isEnabled.current and (manager->numJoinedRoadblockVehicles >= roadblockJoinLimit.value.current)) return false;

			return (chasersAreIndependent.current or (ChasersManager::GetGlobalNumNonRoadblockVehicles() < activeChaserLimit.max.current));
		}


		[[nodiscard]] static const char* __fastcall GetNameOfNewChaser(const address pursuit)
		{
			const auto* const manager = ChasersManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return nullptr);

			return (manager->MayNewChaserSpawn()) ? manager->chaserSpawns.GetNameOfAvailableCop() : nullptr;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] vault __fastcall ReplaceCutsceneVehicleType(vault type)
	{
		constexpr auto ReplaceSupport = [](const auto& vehicle, const vault type) -> vault
		{
			return (GroundSupport::anyFeatureEnabled) ? Globals::GetVaultHash(vehicle.current) : type;
		};

		switch (type)
		{
		case     "copsportghost"_vlt:
		case    "copmidsize_nis"_vlt:
		case "copmidsize_nis_ld"_vlt:
			break; // replace with table

		case "copsuv"_vlt:
			return ReplaceSupport(GroundSupport::heavy3LightVehicle, type);

		case "copcross"_vlt:
			return ReplaceSupport(GroundSupport::leader5CrossVehicle, type);

		default:
			return type; // keep unchanged
		}

		const char* const copName = sceneSpawns.GetNewNameOfAvailableCopWithFallback();

		if (not copName)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Failed to replace", Globals::VehicleType::GetName(type));

			ASSERT_UNREACHABLE_THEN(return type);
		}

		type = Globals::GetVaultHash(copName);

		sceneSpawns.IncrementByType(type);

		return type;
	}



	[[nodiscard]] bool IsEventActive()
	{
		ASSERT_CONDITION_THEN_IF_FALSE(Globals::raceStatus, return false);

		return (AsReference<int>(Globals::raceStatus + 0x1960) != 0);
	}



	[[nodiscard]] const char* __fastcall GetNameOfNewNonChaser(const address caller)
	{
		switch (caller)
		{
		case 0x42EAAD: // first cop of milestone / bounty pursuit
			return patrolSpawns.GetNameOfAvailableCopWithFallback();

		case 0x430DAD: // free patrol
			return patrolSpawns.GetNameOfAvailableCop();
				
		case 0x43E049: // roadblock
			return roadblockSpawns.GetNewNameOfAvailableCopWithFallback();
		}

		if constexpr (Globals::loggingEnabled)
			Globals::LogWarning(logTag, "Unknown ByClass caller:", caller);

		ASSERT_UNREACHABLE_THEN(return nullptr);
	}



	[[nodiscard]] bool CurrentEventForcesPursuit()
	{
		ASSERT_CONDITION_THEN_IF_FALSE(Globals::raceStatus, return false);

		const auto IsPursuitEvent = AsFunction<bool __thiscall (address)>(0x5FBE70);
		return IsPursuitEvent(AsReference<address>(Globals::raceStatus + 0x1968));
	}



	void __fastcall UpdatePrefetchedCopName(const size_t eventHeatLevel)
	{
		eventHasScriptedPursuit = CurrentEventForcesPursuit();
		usePrefetchedCopName    = eventHasScriptedPursuit;

		if (eventHasScriptedPursuit)
		{
			const size_t safeHeatLevel = HeatParameters::ClampHeatLevel(eventHeatLevel);
			const auto&  spawnTable    = CopSpawnTables::scriptedTable.roam[safeHeatLevel - 1];

			prefetchedCopName = spawnTable.GetNameOfAvailableCop();

			if constexpr (Globals::loggingEnabled)
				Globals::LogTagged(logTag, "First scripted cop:", prefetchedCopName);
		}
		else prefetchedCopName = nullptr;
	}



	void __fastcall ShuffleRoadblockVehicles
	(
		address* const copVehicles,
		const size_t   numCopVehicles
	) {
		Globals::pRNG.Shuffle(std::span(copVehicles, numCopVehicles));
	}



	void ProcessSoftEventReset()
	{
		usePrefetchedCopName = eventHasScriptedPursuit;

		patrolSpawns   .Clear();
		scriptedSpawns .Clear();
		roadblockSpawns.Clear();
	}



	void ProcessHardEventReset()
	{
		eventHasScriptedPursuit = false;
		prefetchedCopName       = nullptr;

		ProcessSoftEventReset();
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Notifies "Chasers" managers of arriving waves of backup cops
	ASSEMBLY_DETOUR(WaveReset, /* begin = */ 0x40A9E9, /* end = */ 0x40A9F3)
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0x14C], 0 // blackup flag

			mov ecx, esi
			call ChasersManager::NotifyOfWaveReset // ecx: pursuit

			EXIT_ASSEMBLY_DETOUR(WaveReset)
		}
	}



	// Processes join requests from roadblock vehicles
	ASSEMBLY_DETOUR(JoinRequest, 0x4443AE, 0x4443B6)
	{
		__asm
		{
			cmp dword ptr [esi + 0xB8], edi
			je conclusion // no vehicle(s)

			lea ecx, dword ptr [esi + 0x40]
			call ChasersManager::MayRoadblockVehicleJoin // ecx: pursuit
			test al, al

			mov eax, dword ptr [esi + 0xB8]

			conclusion:
			EXIT_ASSEMBLY_DETOUR(JoinRequest)
		}
	}



	// Registers successful "Patrols" spawns
	ASSEMBLY_DETOUR(PatrolSpawn, 0x430E37, 0x430E3D)
	{
		__asm
		{
			push edi // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::Increment

			// Execute original code and resume
			inc dword ptr [ebp + 0x94]

			EXIT_ASSEMBLY_DETOUR(PatrolSpawn)
		}
	}



	// Sets the minimum spawn distance between "Chasers"
	ASSEMBLY_DETOUR(CopClearance, 0x41A139, 0x41A13F)
	{
		__asm
		{
			mov edx, offset squaredChaserSpawnClearance
			mov eax, 0x891064 // pointer to vanilla value

			cmp byte ptr [esp + 0x2C], 0
			cmovne edx, eax // not "Chasers" cop

			fcomp dword ptr [edx]

			EXIT_ASSEMBLY_DETOUR(CopClearance)
		}
	}



	// Registers successful "Scripted" spawns
	ASSEMBLY_DETOUR(ScriptedSpawn, 0x42E8A8, 0x42E8AF)
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			push esi // copVehicle
			mov ecx, offset scriptedSpawns
			call Contingent::Increment

			mov al, 1 // restore value

			mov ecx, dword ptr [esi + 0x54]       // AIVehicle
			mov byte ptr [ecx - 0x4C + 0x76B], al // padding byte: "Scripted" flag

			mov byte ptr [usePrefetchedCopName], 0 // no longer needed

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x314]

			EXIT_ASSEMBLY_DETOUR(ScriptedSpawn)
		}
	}



	// Unregisters "Patrols" joining pursuits
	ASSEMBLY_DETOUR(PatrolPursuit, 0x4224B0, 0x4224B6)
	{
		using enum VaultHash;

		__asm
		{
			cmp eax, AIGOALPATROL
			jne conclusion // not patrol goal

			mov eax, dword ptr [edi + 0x4C - 0x4] // vehicle
			cmp dword ptr [eax + 0x94], 2         // driver class
			jne conclusion                        // not cop

			cmp byte ptr [edi + 0x76B], 1 // padding byte: "Scripted" flag
			je conclusion                 // is "Scripted" cop

			push eax // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::Decrement

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [edi + 0xB8]

			EXIT_ASSEMBLY_DETOUR(PatrolPursuit)
		}
	}



	// Unregisters despawned "Patrols"
	ASSEMBLY_DETOUR(PatrolDespawn, 0x415E03, 0x415E08)
	{
		using enum VaultHash;

		__asm
		{
			cmp dword ptr [esi + 0x78], AIGOALPATROL
			jne conclusion // not patrol goal

			mov eax, dword ptr [esi - 0x4] // vehicle
			cmp dword ptr [eax + 0x94], 2  // driver class
			jne conclusion                 // not cop

			cmp byte ptr [esi - 0x4C + 0x76B], 1 // padding byte: "Scripted" flag
			je conclusion                        // is "Scripted" cop

			push eax // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::Decrement

			conclusion:
			// Execute original code and resume
			xor eax, eax

			EXIT_ASSEMBLY_DETOUR(PatrolDespawn)
		}
	}



	// Selects the spawn-location algorithm for "Chasers"
	ASSEMBLY_DETOUR(SpawnLocation, 0x430E7B, 0x430E9F)
	{
		__asm
		{
			push ebx
			push ebp
			push esi
			push edi

			mov dword ptr [esp + 0x38], ecx
			mov esi, dword ptr [esp + 0x380]

			mov bl, byte ptr [chaserSpawnDistance.isEnabled.current]

			mov edx, dword ptr [esi]
			mov ecx, esi

			EXIT_ASSEMBLY_DETOUR(SpawnLocation)
		}
	}

	

	// Determines the spawn distance for new "Chasers"
	ASSEMBLY_DETOUR(SpawnDistance, 0x431419, 0x431426)
	{
		__asm
		{
			push ecx

			mov ecx, offset chaserSpawnDistance.interval
			call HeatParameters::Interval<float>::GetRandomValue

			EXIT_ASSEMBLY_DETOUR(SpawnDistance)
		}
	}
	


	// Notifies "Roadblocks" contingent of successful "Roadblocks" spawns
	ASSEMBLY_DETOUR(RoadblockSpawn, 0x43E04F, 0x43E06C)
	{
		static constexpr address AddVehicleToRoadblock = 0x43C4E0;
		static constexpr address generationExit        = 0x43E031;

		__asm
		{
			je conclusion // spawn intended to fail

			push eax

			push eax // copVehicle
			mov ecx, offset roadblockSpawns
			call Contingent::Increment

			pop ecx
			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x80] // PVehicle::Activate

			lea eax, dword ptr [esp + 0x1C]

			push eax
			lea ecx, dword ptr [esp + 0x48]
			call dword ptr [AddVehicleToRoadblock]

			conclusion:
			dec edi
			jne generation // car(s) left to generate

			mov ecx, offset roadblockSpawns
			call Contingent::Clear

			EXIT_ASSEMBLY_DETOUR(RoadblockSpawn)

			generation:
			jmp dword ptr [generationExit]
		}
	}



	// Decides whether the game may spawn more traffic cars
	ASSEMBLY_DETOUR(TrafficDensity, 0x426C4E, 0x426C6A)
	{
		__asm
		{
			cmp byte ptr [trafficIgnoresChasers.current], 1
			je roadblock // "Chasers" ignored

			mov ecx, edi
			call ChasersManager::IsChaserAvailable // ecx: pursuit
			test al, al
			jne conclusion                         // pending "Chasers" spawn

			roadblock:
			cmp byte ptr [trafficIgnoresRoadblocks.current], 1
			je conclusion // roadblocks ignored

			cmp byte ptr [edi + 0x190], 0 // request flag

			conclusion:
			EXIT_ASSEMBLY_DETOUR(TrafficDensity)
		}
	}



	// Marks cop vehicles created outside of free-roam pursuits
	ASSEMBLY_DETOUR(CopConstructor, 0x41EE72, 0x41EE7C)
	{
		__asm
		{
			call IsEventActive
			mov byte ptr [esi + 0xA9], al // padding byte: creation context

			// Execute original code and resume
			mov eax, dword ptr [esi + 0x54]

			EXIT_ASSEMBLY_DETOUR(CopConstructor)
		}
	}



	// Ensures only cops of same origin get recycled
	ASSEMBLY_DETOUR(RecyclingCheck, 0x41ED5D, 0x41ED66)
	{
		__asm
		{
			// Execute original code first
			mov ecx, dword ptr [esp + 0x78]
			add esp, 0x4
			cmp eax, ecx
			jne conclusion // type mismatch

			call IsEventActive
			cmp al, byte ptr [esi + 0xA9] // padding byte: creation context

			conclusion:
			EXIT_ASSEMBLY_DETOUR(RecyclingCheck)
		}
	}



	// Intercepts the game's requests for cop vehicles by class
	ASSEMBLY_DETOUR(ByClassRequest, 0x426610, 0x426730)
	{
		__asm
		{
			mov dword ptr [esp + 0x4], ecx

			mov ecx, dword ptr [esp]
			call GetNameOfNewNonChaser // ecx: caller
			test eax, eax
			je conclusion              // no replacement

			push eax
			mov ecx, dword ptr [esp + 0x8]
			call Globals::GetAvailableCopVehicleByName

			conclusion:
			EXIT_ASSEMBLY_DETOUR(ByClassRequest)
		}
	}



	// Selects the vehicles for in-game cutscenes
	ASSEMBLY_DETOUR(CutsceneVehicle, 0x6F316C, 0x6F3176)
	{
		__asm
		{
			mov edx, dword ptr [esp + 0x80]

			mov ecx, dword ptr [edx + ebx * 0x4]
			call ReplaceCutsceneVehicleType // ecx: type
			mov esi, eax

			EXIT_ASSEMBLY_DETOUR(CutsceneVehicle)
		}
	}

	

	// Replaces "Scripted" cop vehicles
	ASSEMBLY_DETOUR(ScriptedRequest, 0x42E718, 0x42E721)
	{
		__asm
		{
			// execute original code first
			mov dword ptr [esp + 0x28], eax
			lea esi, dword ptr [edi + 0x18]

			cmp byte ptr [eventHasScriptedPursuit], 0
			je replacement // not scripted pursuit

			cmp byte ptr [usePrefetchedCopName], 0
			je replacement // do not use prefetched name

			mov eax, dword ptr [prefetchedCopName]
			test eax, eax
			cmovne esi, eax // prefetched name valid
			jmp conclusion  // use prefetched name

			replacement:
			mov ecx, offset scriptedSpawns
			call Contingent::GetNameOfAvailableCopWithFallback
			mov esi, eax
			
			conclusion:
			mov ecx, ebp

			EXIT_ASSEMBLY_DETOUR(ScriptedRequest)
		}
	}



	// Prefetches name of first scripted cop to spawn in events
	ASSEMBLY_DETOUR(FirstScriptedCop, 0x61E2AE, 0x61E2B7)
	{
		__asm
		{
			mov ecx, dword ptr [eax]
			call UpdatePrefetchedCopName // ecx: eventHeatLevel

			mov eax, dword ptr [prefetchedCopName]
			test eax, eax

			EXIT_ASSEMBLY_DETOUR(FirstScriptedCop)
		}
	}



	// Notifies "Scripted" contingent of finished events
	ASSEMBLY_DETOUR(ScriptedSpawnReset, 0x42E901, 0x42E906)
	{
		__asm
		{
			cmp dword ptr [esi + 0x70], 1 // "Scripted" cops in queue
			jg conclusion                 // was not final spawn

			push eax

			mov ecx, offset scriptedSpawns
			call Contingent::Clear

			pop eax

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [eax]
			mov edx, dword ptr [eax + 0x4]

			EXIT_ASSEMBLY_DETOUR(ScriptedSpawnReset)
		}
	}



	// Shuffles the order of requested roadblock vehicles
	ASSEMBLY_DETOUR(RoadblockShuffling, 0x43E406, 0x43E40E)
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esp + 0x14], 0

			mov ecx, dword ptr [esp + 0x34] 
			mov edx, dword ptr [ebx - 0x4]
			call ShuffleRoadblockVehicles // ecx: copVehicles; edx: numCopVehicles

			EXIT_ASSEMBLY_DETOUR(RoadblockShuffling)
		}
	}



	// Resets the cutscene-cop contingent
	ASSEMBLY_DETOUR(CutsceneConclusion, 0x6F3270, 0x6F3275)
	{
		__asm
		{
			// Execute original code first
			pop ebp
			pop ebx
			add esp, 0x6C

			mov ecx, offset sceneSpawns
			call Contingent::Clear

			EXIT_ASSEMBLY_DETOUR(CutsceneConclusion)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	void ExtractTrackingSettings(const ConfigParser::Parser& parser)
	{
		const auto* const section = parser.GetSection("Board:Tracking");
		if (not section) return; // file missing; keep tracking disabled

		const auto ExtractSetting = [section](const std::string_view key, bool& isTracked) -> void
		{
			ConfigParser::Parser::ExtractScalars<bool>(section, key, {isTracked});

			if constexpr (Globals::loggingEnabled)
			{
				if (isTracked)
					Globals::LogPlain("Tracking", key);
			}
		};

		ExtractSetting("heavyCops",     trackHeavyVehicles);
		ExtractSetting("leaderCops",    trackLeaderVehicles);
		ExtractSetting("roadblockCops", trackRoadblockVehicles);
	}



	void UpdateParameterConversions()
	{
		squaredChaserSpawnClearance = chaserSpawnClearance.current * chaserSpawnClearance.current;
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::fileCarSpawns);

		// Pursuit-board tracking
		ExtractTrackingSettings(parser);

		// Heat parameters (first file)
		HeatParameters::Extract(parser, "Chasers:Limits", activeChaserLimit);

		HeatParameters::Extract(parser, "Chasers:Independence", chasersAreIndependent);

		HeatParameters::Extract(parser, "Chasers:Decrement", onlyDestroyedDecrement);

		HeatParameters::Extract(parser, "Chasers:Backup", transitionTriggersBackup);

		HeatParameters::Extract(parser, "Chasers:Locations", chaserSpawnDistance);

		HeatParameters::Extract(parser, "Chasers:Clearance", chaserSpawnClearance);

		HeatParameters::Extract(parser, "Traffic:Independence", trafficIgnoresChasers, trafficIgnoresRoadblocks);

		// Heat parameters (second file)
		parser.ParseFile(Globals::pathAdvanced, Globals::fileRoadblocks);

		HeatParameters::Extract(parser, "Joining:Limit", roadblockJoinLimit);

		// Parameter conversions
		UpdateParameterConversions(); // uses vanilla value(s)

		// Container pre-allocations
		patrolSpawns   .Reserve(20);
		scriptedSpawns .Reserve(10);
		roadblockSpawns.Reserve(10);

		// Code modifications 
		MemoryTools::Write<byte>(0x00, {0x433CB2}); // min. displayed count
		MemoryTools::Write<byte>(0x90, {0x4443E4}); // roadblock increment

		MemoryTools::Write<vault>("ForceHeatLevel"_vlt, {0x61E295}); // "CopSpawnType" query

		MemoryTools::MakeRangeNOP<0x43EB84, 0x43EB92>(); // global spawn-limit check
		MemoryTools::MakeRangeNOP<0x4442AC, 0x4442C2>(); // zero-wave / capacity increment
		MemoryTools::MakeRangeNOP<0x57B186, 0x57B189>(); // helicopter           increment
		MemoryTools::MakeRangeNOP<0x42B74E, 0x42B771>(); // cops-lost            increment
		MemoryTools::MakeRangeNOP<0x4440D7, 0x4440DF>(); // membership check

		MemoryTools::MakeRangeJMP<0x42BA50, 0x42BCEE>(ChasersManager::GetNameOfNewChaser); // AIPursuit::CopRequest

		PATCH_ASSEMBLY_DETOUR(WaveReset);
		PATCH_ASSEMBLY_DETOUR(JoinRequest);
		PATCH_ASSEMBLY_DETOUR(PatrolSpawn);
		PATCH_ASSEMBLY_DETOUR(CopClearance);
		PATCH_ASSEMBLY_DETOUR(ScriptedSpawn);
		PATCH_ASSEMBLY_DETOUR(PatrolPursuit);
		PATCH_ASSEMBLY_DETOUR(PatrolDespawn);
		PATCH_ASSEMBLY_DETOUR(SpawnLocation);
		PATCH_ASSEMBLY_DETOUR(SpawnDistance);
		PATCH_ASSEMBLY_DETOUR(RoadblockSpawn);
		PATCH_ASSEMBLY_DETOUR(TrafficDensity);
		PATCH_ASSEMBLY_DETOUR(CopConstructor);
		PATCH_ASSEMBLY_DETOUR(RecyclingCheck);
		PATCH_ASSEMBLY_DETOUR(ByClassRequest);
		PATCH_ASSEMBLY_DETOUR(CutsceneVehicle);
		PATCH_ASSEMBLY_DETOUR(ScriptedRequest);
		PATCH_ASSEMBLY_DETOUR(FirstScriptedCop);
		PATCH_ASSEMBLY_DETOUR(ScriptedSpawnReset);
		PATCH_ASSEMBLY_DETOUR(CutsceneConclusion);
		PATCH_ASSEMBLY_DETOUR(RoadblockShuffling);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		// Vehicle contingents
		sceneSpawns    .UpdateSpawnTable();
		patrolSpawns   .UpdateSpawnTable();
		scriptedSpawns .UpdateSpawnTable();
		roadblockSpawns.UpdateSpawnTable();

		// Heat parameters
		activeChaserLimit.SetToHeatState(state);

		chasersAreIndependent.SetToHeatState(state);

		onlyDestroyedDecrement.SetToHeatState(state);

		transitionTriggersBackup.SetToHeatState(state);

		chaserSpawnDistance.SetToHeatState(state);

		chaserSpawnClearance.SetToHeatState(state);

		trafficIgnoresChasers   .SetToHeatState(state);
		trafficIgnoresRoadblocks.SetToHeatState(state);

		roadblockJoinLimit.SetToHeatState(state);

		// Parameter conversions
		UpdateParameterConversions();
	}



	void NotifyOfSoftEventReset()
	{
		if (not anyFeatureEnabled) return;

		ProcessSoftEventReset();
	}



	void NotifyOfHardEventReset()
	{
		if (not anyFeatureEnabled) return;

		ProcessHardEventReset();
	}
}