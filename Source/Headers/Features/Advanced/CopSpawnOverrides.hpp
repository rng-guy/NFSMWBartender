#pragma once

#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"
#include "../../Common/PersistentStrings.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "../../Features/Basic/GeneralSettings.hpp"

#include "CopSpawnTables.hpp"
#include "PursuitFeatures.hpp"
#include "HelicopterOverrides.hpp"



namespace CopSpawnOverrides
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[SPA]";
	constexpr LogLiteral logName = "CopSpawnOverrides";





	// Contingent class -----------------------------------------------------------------------------------------------------------------------------

	#define COP_CONTINGENT(name, ...) CopSpawnOverrides::Contingent name{#name, __VA_ARGS__}

	class Contingent
	{
	private: // aliases

		using TablePointer = HeatParameters::Pointer<CopSpawnTables::SpawnTable>;


	private: // members

		int numTotalActiveCops = 0;

		const address             pursuit; // pursuit-locked and immobile
		const TablePointer* const source;  // reference would break constinit in MSVC
		
		mutable const char* cachedCopName = nullptr; // only cleared on spawn success

		CopSpawnTables::SpawnTable table;

		ModContainers::VaultMap<int> copTypeToNumActive;

		[[no_unique_address]] LogLiteral name;


	private: // methods

		bool ChangeNumActiveCops
		(
			const vault copType, 
			const int   change
		) {
			if (change == 0) return true;

			const auto [pairIt, isNewType] = this->copTypeToNumActive.try_emplace(copType, 0);

			int& numActiveCops = pairIt->second;

			numActiveCops            += change;
			this->numTotalActiveCops += change;

			this->table.ChangeNumActiveCops(copType, change);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
				{
					Globals::LogPlain("Type ratio:", numActiveCops, '/', this->numTotalActiveCops);
					Globals::LogPlain("Available: ", this->table.GetNumAvailableCops(copType));
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
			const LogLiteral    name, 
			const TablePointer& source
		) 
			: name(name), source(&source), pursuit(0x0), table()
		{
		}


		Contingent
		(
			const LogLiteral    name,
			const TablePointer& source,
			const address       pursuit
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
					{
						if (const auto* const copName = PersistentStrings::Get(copType))
							Globals::LogPlain("Copied", numActiveCops, *copName);

						else Globals::LogPlain("Copied", numActiveCops, copType);
					}
				}

				this->table.ChangeNumActiveCops(copType, numActiveCops);
			}
		}


		void ClearVehicles()
		{
			this->numTotalActiveCops = 0;

			this->cachedCopName = nullptr;
			this->table.ResetActiveCopCounts();

			this->copTypeToNumActive.clear();
		}


		void AddVehicleByType(const vault copType)
		{
			this->cachedCopName = nullptr; // almost always matches copType

			this->ChangeNumActiveCops(copType, /* change = */ +1);
		}


		void AddVehicle(const address copVehicle)
		{
			this->AddVehicleByType(Globals::GetVehicleType(copVehicle));
		}


		bool RemoveVehicleByType(const vault copType)
		{
			return this->ChangeNumActiveCops(copType, /* change = */ -1);
		}


		bool RemoveVehicle(const address copVehicle)
		{
			return this->RemoveVehicleByType(Globals::GetVehicleType(copVehicle));
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


		[[nodiscard]] const char* GetNameOfAvailableCopWithFallback() const
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





	// Parameters (cont.) ---------------------------------------------------------------------------------------------------------------------------

	// Pursuit-board tracking
	bool trackHeavyVehicles     = false;
	bool trackLeaderVehicles    = false;
	bool trackRoadblockVehicles = false;

	// Heat parameters
	constinit HEAT_PARAMETER_INTERVAL(int, activeChaserLimit, 1, 8, {0}); // cars

	constinit HEAT_PARAMETER_VALUE(bool, chasersAreIndependent, false);

	constinit HEAT_PARAMETER_VALUE(bool, onlyDestroyedDecrement, false);

	constinit HEAT_PARAMETER_VALUE(bool, transitionTriggersBackup, false);

	constinit HEAT_PARAMETER_VALUE(float, chaserSpawnClearance, 40.f, {0.f}); // metres

	constinit HEAT_PARAMETER_VALUE(bool, trafficIgnoresChasers,    false);
	constinit HEAT_PARAMETER_VALUE(bool, trafficIgnoresRoadblocks, false);

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(int, roadblockJoinLimit, {0}); // cars

	// Parameter conversions
	float squaredChaserSpawnClearance; // metres²

	// Inline hashes for ASM
	enum class VaultHash : vault
	{
		AIGOALPATROL = "AIGoalPatrol"_vlt
	};

	// Code caves
	RELEASE_CONSTINIT COP_CONTINGENT(patrolSpawns,    CopSpawnTables::patrolSpawnTable);
	RELEASE_CONSTINIT COP_CONTINGENT(scriptedSpawns,  CopSpawnTables::scriptedSpawnTable);
	RELEASE_CONSTINIT COP_CONTINGENT(roadblockSpawns, CopSpawnTables::roadblockSpawnTable);

	bool        eventHasScriptedPursuit = false;   // scripted free-roam pursuits request a cop before they know their Heat level,
	bool        usePrefetchedCopName    = false;   // so we must prefetch a valid cop name using their event's Heat level instead
	const char* prefetchedCopName       = nullptr; // (this is completely unrelated to knowing the player vehicle's Heat level)





	// ChasersManager class -------------------------------------------------------------------------------------------------------------------------

	class ChasersManager : public PursuitFeatures::Reaction, public PursuitFeatures::Searchable<ChasersManager>
	{
	private: // members

		bool waveParametersKnown = false;

		int maxNumPatrolCars           = 0;
		int numSupportVehicles         = 0;
		int numTrackedNonChasers       = 0;
		int numJoinedRoadblockVehicles = 0;

		int&   pursuitStatus = AsReference<int>  (this->pursuit + 0x218);
		float& backupTimer   = AsReference<float>(this->pursuit + 0x21C);

		int& fullWaveCapacity       = AsReference<int>(this->pursuit + 0x144);
		int& numCopsLostInWave      = AsReference<int>(this->pursuit + 0x14C);
		int& numCopsToTriggerBackup = AsReference<int>(this->pursuit + 0x148);

		const bool&  isPerpBusted      = AsReference<bool> (this->pursuit + 0xE8);
		const bool&  bailingPursuit    = AsReference<bool> (this->pursuit + 0xE9);
		const bool&  isFreeRoamPursuit = AsReference<bool> (this->pursuit + 0xA8);
		const float& copSpawnCooldown  = AsReference<float>(this->pursuit + 0xCC);

		COP_CONTINGENT(chaserSpawns, CopSpawnTables::chaserSpawnTable, this->pursuit);

		inline static constexpr LogLiteral name = "ChasersManager";


	private: // methods

		void UpdateSpawnTable()
		{
			if (Globals::playerHeatLevelKnown)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Updating table");

				this->chaserSpawns.UpdateSpawnTable();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Skipping updating spawn table");
		}


		void UpdateNumPatrolCars()
		{
			const int* const numPatrolCars = AsPointer<int>(Globals::GetFromPursuitLevel(this->pursuit, "NumPatrolCars"_vlt));

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


		[[nodiscard]] int GetNumTotalMobileCops() const
		{
			return this->chaserSpawns.GetNumTotalActiveCops() + this->numSupportVehicles + this->numJoinedRoadblockVehicles;
		}


		[[nodiscard]] bool MayNewChaserSpawn() const
		{
			if (not Globals::playerHeatLevelKnown) return false;

			if (this->isPerpBusted)           return false;
			if (this->bailingPursuit)         return false;
			if (this->copSpawnCooldown > 0.f) return false;

			if (not this->chaserSpawns.IsAnyCopAvailable()) return false;

			const int numActiveChasers  = this->chaserSpawns.GetNumTotalActiveCops();
			const int numActiveVehicles = (chasersAreIndependent.current) ? numActiveChasers : this->GetNumTotalMobileCops();

			if (numActiveVehicles >= activeChaserLimit.max.current) return false;

			if (Globals::IsPursuitInCooldownMode(this->pursuit))
				return (numActiveChasers < this->maxNumPatrolCars);

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
			const address copAIVehiclePursuit = Globals::GetAIVehiclePursuitOfVehicle(copVehicle);
			ASSERT_CONDITION_THEN_IF_FALSE(copAIVehiclePursuit, return false);

			return AsReference<bool>(copAIVehiclePursuit + 0x22);
		}


		void ProcessAddedChaser(const address copVehicle)
		{
			this->chaserSpawns.AddVehicle(copVehicle);
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
			if (not this->chaserSpawns.RemoveVehicle(copVehicle))
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


		[[nodiscard]] static bool HasRoadblockVehicleCapacity(const address pursuit)
		{
			const auto* const manager = ChasersManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return false);

			bool hasCapacity = (chasersAreIndependent.current or (manager->GetNumTotalMobileCops() < activeChaserLimit.max.current));

			if (roadblockJoinLimit.isEnabled.current)
				hasCapacity &= (manager->numJoinedRoadblockVehicles < roadblockJoinLimit.value.current);

			return hasCapacity;
		}


		[[nodiscard]] static const char* __fastcall GetNameOfNewChaser(const address pursuit)
		{
			const auto* const manager = ChasersManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return nullptr);

			return (manager->MayNewChaserSpawn()) ? manager->chaserSpawns.GetNameOfAvailableCop() : nullptr;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool IsEventActive()
	{
		const address raceStatusObject = AsReference<address>(0x91E000);
		ASSERT_CONDITION_THEN_IF_FALSE(raceStatusObject, return false);

		return (AsReference<int>(raceStatusObject + 0x1960) != 0);
	}



	[[nodiscard]] const char* __fastcall GetNameOfNewNonChaser(const address caller)
	{
		if (Globals::playerHeatLevelKnown)
		{
			switch (caller)
			{
			case 0x4269E6: // helicopter
				return HelicopterOverrides::HelicopterManager::GetHelicopterName();

			case 0x42EAAD: // first cop of milestone / bounty pursuit
				return patrolSpawns.GetNameOfAvailableCopWithFallback();

			case 0x430DAD: // free patrol
				return patrolSpawns.GetNameOfAvailableCop();
				
			case 0x43E049: // roadblock
				return roadblockSpawns.GetNewNameOfAvailableCopWithFallback();
			}

			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Unknown ByClass return address:", caller);

			ASSERT_UNREACHABLE;
		}

		return nullptr;
	}



	[[nodiscard]] bool CurrentEventForcesPursuit()
	{
		const address raceStatus = AsReference<address>(0x91E000);
		ASSERT_CONDITION_THEN_IF_FALSE(raceStatus, return false);

		const auto    IsPursuitEvent = AsFunction <bool __thiscall (address)>(0x5FBE70);
		const address raceParameters = AsReference<address>                  (raceStatus + 0x1968);

		return IsPursuitEvent(raceParameters);
	}



	void __fastcall UpdatePrefetchedCopName(const size_t eventHeatLevel)
	{
		eventHasScriptedPursuit = CurrentEventForcesPursuit();
		usePrefetchedCopName    = eventHasScriptedPursuit;

		if (eventHasScriptedPursuit)
		{
			const size_t safeHeatLevel = HeatParameters::ClampHeatLevel(eventHeatLevel);
			const auto&  spawnTable    = CopSpawnTables::scriptedSpawnTable.roam[safeHeatLevel - 1];
			prefetchedCopName          = spawnTable.GetNameOfAvailableCop();

			if constexpr (Globals::loggingEnabled)
				Globals::LogTagged(logTag, "First scripted cop:", prefetchedCopName);
		}
		else prefetchedCopName = nullptr;
	}



	void ProcessSoftEventReset()
	{
		usePrefetchedCopName = eventHasScriptedPursuit;

		patrolSpawns   .ClearVehicles();
		scriptedSpawns .ClearVehicles();
		roadblockSpawns.ClearVehicles();
	}



	void ProcessHardEventReset()
	{
		eventHasScriptedPursuit = false;
		prefetchedCopName       = nullptr;

		ProcessSoftEventReset();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address waveResetEntrance = 0x40A9E9;
	constexpr address waveResetExit     = 0x40A9F3;

	// Notifies "Chasers" managers of backup waves
	__declspec(naked) void WaveReset()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0x14C], 0 // blackup flag

			mov ecx, esi
			call ChasersManager::NotifyOfWaveReset // ecx: pursuit

			jmp dword ptr [waveResetExit]
		}
	}



	constexpr address patrolSpawnEntrance = 0x430E37;
	constexpr address patrolSpawnExit     = 0x430E3D;

	// Notifies "Patrols" contingent of successful "Patrols" spawns
	__declspec(naked) void PatrolSpawn()
	{
		__asm
		{
			push edi // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::AddVehicle

			// Execute original code and resume
			inc dword ptr [ebp + 0x94] // cops loaded

			jmp dword ptr [patrolSpawnExit]
		}
	}



	constexpr address copClearanceEntrance = 0x41A139;
	constexpr address copClearanceExit     = 0x41A13F;

	// Sets the minimum spawn distance between "Chasers"
	__declspec(naked) void CopClearance()
	{
		__asm
		{
			mov edx, offset squaredChaserSpawnClearance
			mov eax, 0x891064 // pointer to vanilla value

			cmp byte ptr [esp + 0x2C], 0
			cmovne edx, eax // not "Chasers" cop

			fcomp dword ptr [edx]

			jmp dword ptr [copClearanceExit]
		}
	}



	constexpr address copSpawnLimitEntrance = 0x43EB84;
	constexpr address copSpawnLimitExit     = 0x43EB90;

	// Enforces the global cop-spawn limit (if applicable)
	__declspec(naked) void CopSpawnLimit()
	{
		__asm
		{
			xor eax, eax

			cmp byte ptr [chasersAreIndependent.current], 1
			cmovne eax, dword ptr [edi + 0x94] // "Chasers" not independent

			cmp eax, dword ptr [activeChaserLimit.max.current]

			jmp dword ptr [copSpawnLimitExit]
		}
	}



	constexpr address scriptedSpawnEntrance = 0x42E8A8;
	constexpr address scriptedSpawnExit     = 0x42E8AF;

	// Notifies "Scripted" contingent of successful "Scripted" spawns
	__declspec(naked) void ScriptedSpawn()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			push esi // copVehicle
			mov ecx, offset scriptedSpawns
			call Contingent::AddVehicle

			mov al, 1 // restore value

			mov ecx, dword ptr [esi + 0x54]       // AIVehicle
			mov byte ptr [ecx - 0x4C + 0x76B], al // padding byte: "Scripted" flag

			mov byte ptr [usePrefetchedCopName], 0 // no longer needed

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x314]

			jmp dword ptr [scriptedSpawnExit]
		}
	}



	constexpr address patrolPursuitEntrance = 0x4224B0;
	constexpr address patrolPursuitExit     = 0x4224B6;

	// Notifies "Patrols" contingent of "Patrols" joining pursuits
	__declspec(naked) void PatrolPursuit()
	{
		using enum VaultHash;

		__asm
		{
			cmp eax, AIGOALPATROL
			jne conclusion // not patrol goal

			mov eax, dword ptr [edi + 0x4C - 0x4] // PVehicle
			cmp dword ptr [eax + 0x94], 2         // driver class
			jne conclusion                        // not cop

			cmp byte ptr [edi + 0x76B], 1 // padding byte: "Scripted" flag
			je conclusion                 // "Scripted" cop

			push eax // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::RemoveVehicle

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [edi + 0xB8]

			jmp dword ptr [patrolPursuitExit]
		}
	}



	constexpr address patrolDespawnEntrance = 0x415E03;
	constexpr address patrolDespawnExit     = 0x415E08;

	// Notifies "Patrols" contingent of "Patrols" despawns
	__declspec(naked) void PatrolDespawn()
	{
		using enum VaultHash;

		__asm
		{
			cmp dword ptr [esi + 0x78], AIGOALPATROL
			jne conclusion // not patrol goal

			mov eax, dword ptr [esi - 0x4] // PVehicle
			cmp dword ptr [eax + 0x94], 2  // driver class
			jne conclusion                 // not cop

			cmp byte ptr [esi - 0x4C + 0x76B], 1 // padding byte: "Scripted" flag
			je conclusion                        // "Scripted" cop

			push eax // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::RemoveVehicle

			conclusion:
			// Execute original code and resume
			xor eax, eax

			jmp dword ptr [patrolDespawnExit]
		}
	}



	constexpr address roadblockSpawnEntrance = 0x43E04F;
	constexpr address roadblockSpawnExit     = 0x43E06C;

	// Notifies "Roadblocks" contingent of successful "Roadblocks" spawns
	__declspec(naked) void RoadblockSpawn()
	{
		static constexpr address AddVehicleToRoadblock = 0x43C4E0;
		static constexpr address roadblockSpawnSkip    = 0x43E031;

		__asm
		{
			je conclusion // spawn intended to fail

			push eax

			push eax // copVehicle
			mov ecx, offset roadblockSpawns
			call Contingent::AddVehicle

			pop ecx
			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x80] // PVehicle::Activate

			lea eax, dword ptr [esp + 0x1C]
			push eax
			lea ecx, dword ptr [esp + 0x48]
			call dword ptr [AddVehicleToRoadblock]

			conclusion:
			dec edi
			jne skip // car(s) left to generate

			mov ecx, offset roadblockSpawns
			call Contingent::ClearVehicles

			jmp dword ptr [roadblockSpawnExit]

			skip:
			jmp dword ptr [roadblockSpawnSkip]
		}
	}



	constexpr address trafficDensityEntrance = 0x426C4E;
	constexpr address trafficDensityExit     = 0x426C6A;

	// Decides whether the game may spawn more traffic cars
	__declspec(naked) void TrafficDensity()
	{
		__asm
		{
			cmp byte ptr [trafficIgnoresChasers.current], 1
			je roadblock // "Chasers" ignored

			cmp byte ptr [chasersAreIndependent.current], 1
			je chasers // "Chasers" independent

			mov eax, dword ptr [ebx - 0x54 + 0x94] // cops loaded
			cmp eax, dword ptr [activeChaserLimit.max.current]
			jge roadblock                          // at or above spawn limit

			chasers:
			mov ecx, edi
			call ChasersManager::IsChaserAvailable // ecx: pursuit
			test al, al
			jne conclusion                         // pending "Chasers" spawn

			roadblock:
			cmp byte ptr [trafficIgnoresRoadblocks.current], 1
			je conclusion // roadblocks ignored

			cmp byte ptr [edi + 0x190], 0 // roadblock pending

			conclusion:
			jmp dword ptr [trafficDensityExit]
		}
	}



	constexpr address copConstructorEntrance = 0x41EE72;
	constexpr address copConstructorExit     = 0x41EE7C;

	// Marks cop vehicles created outside of free-roam pursuits
	__declspec(naked) void CopConstructor()
	{
		__asm
		{
			call IsEventActive
			mov byte ptr [esi + 0xA9], al // padding byte: creation context

			// Execute original code and resume
			mov eax, dword ptr [esi + 0x54]

			jmp dword ptr [copConstructorExit]
		}
	}



	constexpr address recyclingCheckEntrance = 0x41ED5D;
	constexpr address recyclingCheckExit     = 0x41ED66;

	// Ensures that only free-roam cops can be recycled for free-roam cops (etc.)
	__declspec(naked) void RecyclingCheck()
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
			jmp dword ptr [recyclingCheckExit]
		}
	}



	constexpr address byClassRequestEntrance = 0x426610;
	constexpr address byClassRequestExit     = 0x426730;

	// Intercepts the game's requests for cop vehicles by class
	__declspec(naked) void ByClassRequest()
	{
		static constexpr address GetAvailableCopVehicleByName = 0x41ECD0;

		__asm
		{
			mov dword ptr [esp + 0x4], ecx

			mov ecx, dword ptr [esp]
			call GetNameOfNewNonChaser // ecx: caller
			test eax, eax
			je conclusion              // no replacement

			push eax
			mov ecx, dword ptr [esp + 0x8]
			call dword ptr [GetAvailableCopVehicleByName]

			conclusion:
			jmp dword ptr [byClassRequestExit]
		}
	}



	constexpr address scriptedRequestEntrance = 0x42E718;
	constexpr address scriptedRequestExit     = 0x42E721;

	// Replaces "Scripted" cop vehicles
	__declspec(naked) void ScriptedRequest()
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
			jmp conclusion  // prefetched name used

			replacement:
			cmp byte ptr [Globals::playerHeatLevelKnown], 1
			jne conclusion // Heat level unknown

			mov ecx, offset scriptedSpawns
			call Contingent::GetNameOfAvailableCopWithFallback
			mov esi, eax
			
			conclusion:
			mov ecx, ebp

			jmp dword ptr [scriptedRequestExit]
		}
	}



	constexpr address firstScriptedCopEntrance = 0x61E2AE;
	constexpr address firstScriptedCopExit     = 0x61E2B7;

	// Prefetches name of first scripted cop to spawn in events
	__declspec(naked) void FirstScriptedCop()
	{
		__asm
		{
			mov ecx, dword ptr [eax]
			call UpdatePrefetchedCopName // ecx: eventHeatLevel

			mov eax, dword ptr [prefetchedCopName]
			test eax, eax

			jmp dword ptr [firstScriptedCopExit]
		}
	}



	constexpr address scriptedSpawnResetEntrance = 0x42E901;
	constexpr address scriptedSpawnResetExit     = 0x42E906;

	// Notifies "Scripted" contingent of finished events
	__declspec(naked) void ScriptedSpawnReset()
	{
		__asm
		{
			cmp dword ptr [esi + 0x70], 1 // "Scripted" cops in queue
			jg conclusion                 // was not final spawn

			push eax

			mov ecx, offset scriptedSpawns
			call Contingent::ClearVehicles

			pop eax

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [eax]
			mov edx, dword ptr [eax + 0x4]

			jmp dword ptr [scriptedSpawnResetExit]
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

	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(HeatParameters::configPathAdvanced, "CarSpawns.ini");

		// Pursuit-board tracking
		ExtractTrackingSettings(parser);

		// Heat parameters (first file)
		HeatParameters::Extract(parser, "Chasers:Limits", activeChaserLimit);

		HeatParameters::Extract(parser, "Chasers:Independence", chasersAreIndependent);

		HeatParameters::Extract(parser, "Chasers:Decrement", onlyDestroyedDecrement);

		HeatParameters::Extract(parser, "Chasers:Backup", transitionTriggersBackup);

		HeatParameters::Extract(parser, "Chasers:Clearance", chaserSpawnClearance);

		HeatParameters::Extract(parser, "Traffic:Independence", trafficIgnoresChasers, trafficIgnoresRoadblocks);

		// Heat parameters (second file)
		parser.ParseFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

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

		MemoryTools::Write<word>(0x517D, {0x43EB90}); // undo count skip of "OpenLimitAdjuster"

		MemoryTools::Write<vault>(0xE4211F4F, {0x61E295}); // query "ForceHeatLevel" instead

		MemoryTools::MakeRangeNOP<0x4442AC, 0x4442C2>(); // zero-wave / capacity increment
		MemoryTools::MakeRangeNOP<0x57B186, 0x57B189>(); // helicopter increment
		MemoryTools::MakeRangeNOP<0x42B74E, 0x42B771>(); // cops-lost increment
		MemoryTools::MakeRangeNOP<0x4440D7, 0x4440DF>(); // membership check

		MemoryTools::MakeRangeJMP<0x42BA50, 0x42BCEE>(ChasersManager::GetNameOfNewChaser); // replaces game function

		MemoryTools::MakeRangeJMP<waveResetEntrance,          waveResetExit>         (WaveReset);
		MemoryTools::MakeRangeJMP<patrolSpawnEntrance,        patrolSpawnExit>       (PatrolSpawn);
		MemoryTools::MakeRangeJMP<copClearanceEntrance,       copClearanceExit>      (CopClearance);
		MemoryTools::MakeRangeJMP<copSpawnLimitEntrance,      copSpawnLimitExit>     (CopSpawnLimit);
		MemoryTools::MakeRangeJMP<scriptedSpawnEntrance,      scriptedSpawnExit>     (ScriptedSpawn);
		MemoryTools::MakeRangeJMP<patrolPursuitEntrance,      patrolPursuitExit>     (PatrolPursuit);
		MemoryTools::MakeRangeJMP<patrolDespawnEntrance,      patrolDespawnExit>     (PatrolDespawn);
		MemoryTools::MakeRangeJMP<roadblockSpawnEntrance,     roadblockSpawnExit>    (RoadblockSpawn);
		MemoryTools::MakeRangeJMP<trafficDensityEntrance,     trafficDensityExit>    (TrafficDensity);
		MemoryTools::MakeRangeJMP<copConstructorEntrance,     copConstructorExit>    (CopConstructor);
		MemoryTools::MakeRangeJMP<recyclingCheckEntrance,     recyclingCheckExit>    (RecyclingCheck);
		MemoryTools::MakeRangeJMP<byClassRequestEntrance,     byClassRequestExit>    (ByClassRequest);
		MemoryTools::MakeRangeJMP<scriptedRequestEntrance,    scriptedRequestExit>   (ScriptedRequest);
		MemoryTools::MakeRangeJMP<firstScriptedCopEntrance,   firstScriptedCopExit>  (FirstScriptedCop);
		MemoryTools::MakeRangeJMP<scriptedSpawnResetEntrance, scriptedSpawnResetExit>(ScriptedSpawnReset);

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
		patrolSpawns   .UpdateSpawnTable();
		scriptedSpawns .UpdateSpawnTable();
		roadblockSpawns.UpdateSpawnTable();

		// Heat parameters
		activeChaserLimit.SetToHeatState(state);

		chasersAreIndependent.SetToHeatState(state);

		onlyDestroyedDecrement.SetToHeatState(state);

		transitionTriggersBackup.SetToHeatState(state);

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