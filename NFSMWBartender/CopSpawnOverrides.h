#pragma once

#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "GeneralSettings.h"

#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "HelicopterOverrides.h"



namespace CopSpawnOverrides
{

	// Contingent class -----------------------------------------------------------------------------------------------------------------------------

	class Contingent
	{
	private:

		const CopSpawnTables::SpawnTable* const& source;
		const address                            pursuit;

		int numActiveCops = 0;

		CopSpawnTables::SpawnTable table = *(this->source);

		HashContainers::VaultMap<int> copTypeToCurrentCount;


		void UpdateSpawnTableCapacity
		(
			const vault copType, 
			const int   amount
		) {
			const bool copTypeInSpawnTable = this->table.UpdateCapacity(copType, amount);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
				{
					if (copTypeInSpawnTable)
						Globals::logger.Log(this->pursuit, "[CON] Type capacity:", this->table.GetCapacity(copType));

					else
						Globals::logger.Log(this->pursuit, "[CON] Type capacity undefined");
				}
			}
		}



	public:

		explicit Contingent
		(
			const CopSpawnTables::SpawnTable* const& source,
			const address                            pursuit = 0x0
		) 
			: source(source), pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log<2>('+', this, "Contingent");
			}
		}


		explicit Contingent(const Contingent&)   = delete;
		Contingent& operator=(const Contingent&) = delete;

		explicit Contingent(Contingent&&)   = delete;
		Contingent& operator=(Contingent&&) = delete;


		~Contingent()
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log<2>('-', this, "Contingent");
			}
		}


		void UpdateSpawnTable()
		{
			this->table = *(this->source);

			for (const auto& pair : this->copTypeToCurrentCount)
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (this->pursuit)
					{
						const char* const copName = this->table.ConvertTypeToName(pair.first);

						if (copName)
							Globals::logger.Log(this->pursuit, "[CON] Copying", pair.second, copName);

						else
							Globals::logger.Log(this->pursuit, "[CON] Copying", pair.second, pair.first);
					}
				}

				this->UpdateSpawnTableCapacity(pair.first, -(pair.second));
			}
		}


		void ClearVehicles()
		{
			for (const auto& pair : this->copTypeToCurrentCount)
				this->table.UpdateCapacity(pair.first, pair.second);

			this->copTypeToCurrentCount.clear();
			this->numActiveCops = 0;
		}


		void AddVehicleByType(const vault copType)
		{
			const auto [pair, wasAdded] = this->copTypeToCurrentCount.try_emplace(copType, 1);

			if (not wasAdded)
				++(pair->second);

			this->UpdateSpawnTableCapacity(copType, -1);
			++(this->numActiveCops);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log(this->pursuit, "[CON] Type ratio:", pair->second, '/', this->numActiveCops);
			}
		}


		void AddVehicleByName(const char* const copName)
		{
			this->AddVehicleByType(Globals::GetVaultKey(copName));
		}


		void AddVehicle(const address copVehicle)
		{
			this->AddVehicleByType(Globals::GetVehicleType(copVehicle));
		}


		bool RemoveVehicleByType(const vault copType)
		{
			const auto foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType != this->copTypeToCurrentCount.end())
			{
				--(foundType->second);
				
				this->UpdateSpawnTableCapacity(copType, +1);
				--(this->numActiveCops);

				if constexpr (Globals::loggingEnabled)
				{
					if (this->pursuit)
						Globals::logger.Log(this->pursuit, "[CON] Type ratio:", foundType->second, '/', this->numActiveCops);
				}

				if (foundType->second < 1)
					this->copTypeToCurrentCount.erase(foundType);

				return true;
			}
			
			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log("WARNING: [CON] Unknown type", copType, "in", this->pursuit);
			}

			return false;
		}


		bool RemoveVehicleByName(const char* const copName)
		{
			return this->RemoveVehicleByType(Globals::GetVaultKey(copName));
		}


		bool RemoveVehicle(const address copVehicle)
		{
			return this->RemoveVehicleByType(Globals::GetVehicleType(copVehicle));
		}


		int GetNumActiveCops() const
		{
			return this->numActiveCops;
		}


		bool HasAvailableCop() const
		{
			return this->table.HasCapacity();
		}


		const char* GetNameOfAvailableCop() const
		{
			return this->table.GetRandomAvailableCop();
		}


		const char* GetNameOfSpawnableCop() const
		{
			const char* const copName = this->GetNameOfAvailableCop();
			return (copName) ? copName : this->source->GetRandomAvailableCop();
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Pursuit-board tracking
	bool trackHeavyVehicles  = false;
	bool trackLeaderVehicles = false;
	bool trackJoinedVehicles = false;

	// Heat parameters
	HeatParameters::Interval<int>   activeChaserCounts       (1, 8, {0});   // cars
	HeatParameters::Pair    <bool>  chasersAreIndependents   (false);
	HeatParameters::Pair    <bool>  onlyDestroyedDecrements  (false);
	HeatParameters::Pair    <bool>  transitionTriggersBackups(false);
	HeatParameters::Pair    <float> chaserSpawnClearances    (40.f, {0.f}); // metres

	HeatParameters::Pair<bool> trafficIgnoresChasers   (false);
	HeatParameters::Pair<bool> trafficIgnoresRoadblocks(false);

	HeatParameters::OptionalPair<int> roadblockJoinLimits({0}); // cars

	// Code caves
	bool skipScriptedSpawns = true;
	
	Contingent patrolSpawns   (CopSpawnTables::patrolSpawnTables.current);
	Contingent scriptedSpawns (CopSpawnTables::scriptedSpawnTables.current);
	Contingent roadblockSpawns(CopSpawnTables::roadblockSpawnTables.current);

	// Conversions
	float squaredChaserSpawnClearance = chaserSpawnClearances.current * chaserSpawnClearances.current; // sq. metres





	// ChaserManager class --------------------------------------------------------------------------------------------------------------------------

	class ChasersManager : public PursuitFeatures::PursuitReaction
	{
	private:

		bool waveParametersKnown = false;

		int maxNumPatrolCars           = 0;
		int numActiveNonChasers        = 0;
		int numJoinedRoadblockVehicles = 0;

		volatile int&   pursuitStatus = *reinterpret_cast<volatile int*>  (this->pursuit + 0x218);
		volatile float& backupTimer   = *reinterpret_cast<volatile float*>(this->pursuit + 0x21C);

		volatile int& fullWaveCapacity       = *reinterpret_cast<volatile int*>(this->pursuit + 0x144);
		volatile int& numCopsLostInWave      = *reinterpret_cast<volatile int*>(this->pursuit + 0x14C);
		volatile int& numCopsToTriggerBackup = *reinterpret_cast<volatile int*>(this->pursuit + 0x148);

		const volatile bool&  isPerpBusted      = *reinterpret_cast<volatile bool*> (this->pursuit + 0xE8);
		const volatile bool&  bailingPursuit    = *reinterpret_cast<volatile bool*> (this->pursuit + 0xE9);
		const volatile bool&  isFreeRoamPursuit = *reinterpret_cast<volatile bool*> (this->pursuit + 0xA8);
		const volatile float& copSpawnCooldown  = *reinterpret_cast<volatile float*>(this->pursuit + 0xCC);

		Contingent chaserSpawns{CopSpawnTables::chaserSpawnTables.current, this->pursuit};

		inline static HashContainers::AddressMap<ChasersManager*> pursuitToManager;


		void UpdateSpawnTable()
		{
			if (Globals::playerHeatLevelKnown)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SPA] Updating table");

				this->chaserSpawns.UpdateSpawnTable();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SPA] Skipping updating spawn table");
		}


		void UpdateNumPatrolCars()
		{
			const address attribute = Globals::GetFromPursuitlevel(this->pursuit, 0x24F7A1BC); // fetches "NumPatrolCars"

			this->maxNumPatrolCars = (attribute) ? *reinterpret_cast<volatile int*>(attribute) : 0;

			if constexpr (Globals::loggingEnabled)
			{
				if (attribute)
					Globals::logger.Log(this->pursuit, "[SPA] Max. patrol cars:", this->maxNumPatrolCars);

				else
					Globals::logger.Log("WARNING: [SPA] Invalid numPatrolCars pointer in", this->pursuit);
			}
		}


		int GetWaveCapacity() const
		{
			int waveCapacity = this->fullWaveCapacity - (this->numCopsLostInWave + this->chaserSpawns.GetNumActiveCops());

			if (this->waveParametersKnown)
				waveCapacity -= this->numActiveNonChasers;

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
					Globals::logger.Log(this->pursuit, "[SPA] Wave correction:", -waveCapacity);
			}
		}


		int GetNumPersistentCops() const
		{
			return this->chaserSpawns.GetNumActiveCops() + this->numJoinedRoadblockVehicles;
		}


		bool CanNewChaserSpawn() const
		{
			if (not Globals::playerHeatLevelKnown) return false;

			if (this->isPerpBusted)           return false;
			if (this->bailingPursuit)         return false;
			if (this->copSpawnCooldown > 0.f) return false;

			if (not this->chaserSpawns.HasAvailableCop()) return false;

			const int numActiveChasers  = this->chaserSpawns.GetNumActiveCops();
			const int numActiveVehicles = (chasersAreIndependents.current) ? numActiveChasers : this->GetNumPersistentCops();

			if (numActiveVehicles >= activeChaserCounts.maxValues.current) return false;

			if (Globals::IsInCooldownMode(this->pursuit))
				return (numActiveChasers < this->maxNumPatrolCars);

			return ((numActiveChasers < activeChaserCounts.minValues.current) or (this->GetWaveCapacity() > 0));
		}


		bool IsBackUpTimerActive() const
		{
			return (this->pursuitStatus == 1);
		}


		void ForceTriggerBackup() const
		{
			const auto LockInPursuitAttributes = reinterpret_cast<void (__thiscall*)(address)>(0x40A9B0);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SPA] Force-triggering backup");

			if (this->IsBackUpTimerActive())
			{
				this->backupTimer   = 0.f;
				this->pursuitStatus = 0;
			}

			LockInPursuitAttributes(this->pursuit);
		}


		static bool HasVehicleEngaged(const address copVehicle)
		{
			const address copAIVehiclePursuit = ChasersManager::GetAIVehiclePursuit(copVehicle);
			return (copAIVehiclePursuit) ? *reinterpret_cast<volatile bool*>(copAIVehiclePursuit + 0x22) : false;
		}


		static bool IsTrackedOnPursuitBoard(const CopLabel copLabel)
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				return trackHeavyVehicles;

			case CopLabel::LEADER:
				return trackLeaderVehicles;

			case CopLabel::ROADBLOCK:
				return trackJoinedVehicles;
			}

			return false;
		}


		static ChasersManager* FindManager(const address pursuit)
		{
			const auto foundManager = ChasersManager::pursuitToManager.find(pursuit);

			if (foundManager != ChasersManager::pursuitToManager.end())
				return foundManager->second;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] No manager for pursuit", pursuit);

			return nullptr;
		}



	public:

		explicit ChasersManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "ChasersManager");

			this->pursuitToManager.try_emplace(this->pursuit, this);
		}


		~ChasersManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "ChasersManager");

			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnHeatChange() override 
		{
			this->UpdateSpawnTable();
		}


		void UpdateOncePerHeatLevel() override
		{
			this->UpdateNumPatrolCars();

			if (transitionTriggersBackups.current)
				this->ForceTriggerBackup();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			skipScriptedSpawns = false;

			if (copLabel == CopLabel::CHASER)
			{
				this->chaserSpawns.AddVehicle(copVehicle);
				this->CorrectWaveCapacity();
			}
			else if (this->IsTrackedOnPursuitBoard(copLabel))
			{
				++(this->numActiveNonChasers);

				if (this->waveParametersKnown)
				{
					++(this->fullWaveCapacity);
					++(this->numCopsToTriggerBackup);
				}
			}

			if (copLabel == CopLabel::ROADBLOCK)
				++(this->numJoinedRoadblockVehicles);
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::CHASER)
			{
				if (this->chaserSpawns.RemoveVehicle(copVehicle))
				{
					if ((not onlyDestroyedDecrements.current) or Globals::IsVehicleDestroyed(copVehicle))
					{
						const bool trackCopsLost = (this->isFreeRoamPursuit or GeneralSettings::trackCopsLost);

						if (trackCopsLost and this->HasVehicleEngaged(copVehicle))
							++(this->numCopsLostInWave);

						else if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[SPA] No decrement", (trackCopsLost) ? "(radius)" : "(tracking)");
					}
					else if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[SPA] No decrement (wreck)");
				}
				else if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [SPA] Unknown chaser vehicle", copVehicle, "in", this->pursuit);
			}
			else if (this->IsTrackedOnPursuitBoard(copLabel))
			{
				--(this->numActiveNonChasers);

				if (this->waveParametersKnown)
				{
					--(this->fullWaveCapacity);
					--(this->numCopsToTriggerBackup);
				}		
			}

			if (copLabel == CopLabel::ROADBLOCK)
				--(this->numJoinedRoadblockVehicles);
		}


		static void __fastcall NotifyOfWaveReset(const address pursuit)
		{
			ChasersManager* const manager = ChasersManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			if constexpr (Globals::loggingEnabled)
			{
				if (not manager->waveParametersKnown)
					Globals::logger.Log(manager->pursuit, "[SPA] Wave parameters now known");
			}

			manager->waveParametersKnown = true;

			manager->fullWaveCapacity       += manager->numActiveNonChasers;
			manager->numCopsToTriggerBackup += manager->numActiveNonChasers;

			manager->CorrectWaveCapacity();
		}


		static bool __fastcall IsChaserAvailable(const address pursuit)
		{
			const ChasersManager* const manager = ChasersManager::FindManager(pursuit);
			return (manager) ? manager->CanNewChaserSpawn() : false;
		}


		static bool __fastcall HasJoinCapacity(const address pursuit)
		{
			const ChasersManager* const manager = ChasersManager::FindManager(pursuit);
			if (not manager) return false; // should never happen

			const bool joiningAtLimit = (manager->numJoinedRoadblockVehicles >= roadblockJoinLimits.values.current);;

			if (not chasersAreIndependents.current)
			{
				const bool hasGeneralCapacity = (manager->GetNumPersistentCops() < activeChaserCounts.maxValues.current);

				if (roadblockJoinLimits.isEnableds.current)
					return (hasGeneralCapacity and (not joiningAtLimit));

				return hasGeneralCapacity;
			}
			
			return (not (roadblockJoinLimits.isEnableds.current and joiningAtLimit));
		}


		static const char* __fastcall GetNameOfNewChaser(const address pursuit)
		{
			const ChasersManager* const manager = ChasersManager::FindManager(pursuit);
			return (manager and manager->CanNewChaserSpawn()) ? manager->chaserSpawns.GetNameOfAvailableCop() : nullptr;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool IsEventActive()
	{
		const address raceStatusObject = *reinterpret_cast<volatile address*>(0x91E000);
		return (raceStatusObject and *reinterpret_cast<volatile int*>(raceStatusObject + 0x1960));
	}



	const char* __fastcall GetByClassReplacement(const address caller)
	{
		if (Globals::playerHeatLevelKnown)
		{
			switch (caller)
			{
			case 0x4269E6: // helicopter
				return HelicopterOverrides::HelicopterManager::GetHelicopterVehicle();

			case 0x42EAAD: // first cop of milestone / bounty pursuit
				return CopSpawnTables::patrolSpawnTables.current->GetRandomAvailableCop();

			case 0x430DAD: // free patrol
				return patrolSpawns.GetNameOfAvailableCop();
				
			case 0x43E049: // roadblock
				return roadblockSpawns.GetNameOfSpawnableCop();
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Unknown ByClass return address:", caller);
		}

		return nullptr;
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
			mov dword ptr [esi + 0x14C], 0x0 // blackup flag

			mov ecx, esi
			call ChasersManager::NotifyOfWaveReset // ecx: pursuit

			jmp dword ptr waveResetExit
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

			jmp dword ptr patrolSpawnExit
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
			mov eax, 0x891064 // pointer -> 40.f * 40.f

			cmp byte ptr [esp + 0x2C], 0x0
			cmovne edx, eax // not "Chasers" cop

			fcomp dword ptr [edx]

			jmp dword ptr copClearanceExit
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

			cmp byte ptr chasersAreIndependents.current, 0x1
			cmovne eax, dword ptr [edi + 0x94] // "Chasers" not independent

			cmp eax, dword ptr activeChaserCounts.maxValues.current

			jmp dword ptr copSpawnLimitExit
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

			mov al, 0x1

			mov ecx, dword ptr [esi + 0x54]       // AIVehicle
			mov byte ptr [ecx - 0x4C + 0x76B], al // padding byte: "Scripted" flag

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x314]

			jmp dword ptr scriptedSpawnExit
		}
	}



	constexpr address patrolPursuitEntrance = 0x4224B0;
	constexpr address patrolPursuitExit     = 0x4224B6;

	// Notifies "Patrols" contingent of "Patrols" joining pursuits
	__declspec(naked) void PatrolPursuit()
	{
		__asm
		{
			cmp eax, 0x9AF1BF40 // AIGoalPatrol
			jne conclusion      // not patrol goal

			mov eax, dword ptr [edi + 0x4C - 0x4] // PVehicle
			cmp dword ptr [eax + 0x94], 0x2       // driver class
			jne conclusion                        // not cop

			cmp byte ptr [edi + 0x76B], 0x1 // padding byte: "Scripted" flag
			je conclusion                   // "Scripted" cop

			push eax // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::RemoveVehicle

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [edi + 0xB8]

			jmp dword ptr patrolPursuitExit
		}
	}



	constexpr address patrolDespawnEntrance = 0x415E03;
	constexpr address patrolDespawnExit     = 0x415E08;

	// Notifies "Patrols" contingent of "Patrols" despawns
	__declspec(naked) void PatrolDespawn()
	{
		__asm
		{
			cmp dword ptr [esi + 0x78], 0x9AF1BF40 // AIGoalPatrol
			jne conclusion                         // not patrol goal

			mov eax, dword ptr [esi - 0x4]  // PVehicle
			cmp dword ptr [eax + 0x94], 0x2 // driver class
			jne conclusion                  // not cop

			cmp byte ptr [esi - 0x4C + 0x76B], 0x1 // padding byte: "Scripted" flag
			je conclusion                          // "Scripted" cop

			push eax // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::RemoveVehicle

			conclusion:
			// Execute original code and resume
			xor eax, eax

			jmp dword ptr patrolDespawnExit
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
			call dword ptr AddVehicleToRoadblock

			conclusion:
			dec edi
			jne skip // car(s) left to generate

			mov ecx, offset roadblockSpawns
			call Contingent::ClearVehicles

			jmp dword ptr roadblockSpawnExit

			skip:
			jmp dword ptr roadblockSpawnSkip
		}
	}



	constexpr address trafficDensityEntrance = 0x426C4E;
	constexpr address trafficDensityExit     = 0x426C6A;

	// Decides whether the game may spawn more traffic cars
	__declspec(naked) void TrafficDensity()
	{
		__asm
		{
			cmp byte ptr trafficIgnoresChasers.current, 0x1
			je roadblock // "Chasers" ignored

			cmp byte ptr chasersAreIndependents.current, 0x1
			je chasers // "Chasers" independent

			mov eax, dword ptr [ebx - 0x54 + 0x94] // cops loaded
			cmp eax, dword ptr activeChaserCounts.maxValues.current
			jge roadblock                          // at or above spawn limit

			chasers:
			mov ecx, edi
			call ChasersManager::IsChaserAvailable // ecx: pursuit
			test al, al
			jne conclusion                         // pending "Chasers" spawn

			roadblock:
			cmp byte ptr trafficIgnoresRoadblocks.current, 0x1
			je conclusion // roadblocks ignored

			cmp byte ptr [edi + 0x190], 0x0 // roadblock pending

			conclusion:
			jmp dword ptr trafficDensityExit
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

			jmp dword ptr copConstructorExit
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
			jmp dword ptr recyclingCheckExit
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
			call GetByClassReplacement // ecx: caller
			test eax, eax
			je conclusion              // no replacement

			push eax
			mov ecx, dword ptr [esp + 0x8]
			call dword ptr GetAvailableCopVehicleByName

			conclusion:
			jmp dword ptr byClassRequestExit
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

			cmp byte ptr skipScriptedSpawns, 0x0
			jne conclusion // skip "Scripted" spawn

			cmp byte ptr Globals::playerHeatLevelKnown, 0x1
			jne conclusion // Heat level unknown

			mov ecx, offset scriptedSpawns
			call Contingent::GetNameOfSpawnableCop
			mov esi, eax
			
			conclusion:
			mov ecx, ebp

			jmp dword ptr scriptedRequestExit
		}
	}



	constexpr address scriptedSpawnResetEntrance = 0x42E901;
	constexpr address scriptedSpawnResetExit     = 0x42E906;

	// Notifies "Scripted" contingent of finished events
	__declspec(naked) void ScriptedSpawnReset()
	{
		__asm
		{
			cmp dword ptr [esi + 0x70], 0x1 // "Scripted" cops in queue
			jg conclusion                   // not final spawn

			push eax

			mov ecx, offset scriptedSpawns
			call Contingent::ClearVehicles

			pop eax

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [eax]
			mov edx, dword ptr [eax + 0x4]

			jmp dword ptr scriptedSpawnResetExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [SPA] CopSpawnOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "CarSpawns.ini");

		// Pursuit-board tracking
		const std::string section = "Board:Tracking";

		parser.ParseFromFile<bool>(section, "heavyVehicles",  {trackHeavyVehicles});
		parser.ParseFromFile<bool>(section, "leaderVehicles", {trackLeaderVehicles});
		parser.ParseFromFile<bool>(section, "joinedVehicles", {trackJoinedVehicles});

		if constexpr (Globals::loggingEnabled)
		{
			if (trackHeavyVehicles)  Globals::logger.Log<2>("Tracking HeavyStrategy vehicles");
			if (trackLeaderVehicles) Globals::logger.Log<2>("Tracking LeaderStrategy vehicles");
			if (trackJoinedVehicles) Globals::logger.Log<2>("Tracking roadblock vehicles");
		}

		// Heat parameters
		HeatParameters::Parse(parser, "Chasers:Limits",       activeChaserCounts);
		HeatParameters::Parse(parser, "Chasers:Independence", chasersAreIndependents);
		HeatParameters::Parse(parser, "Chasers:Decrement",    onlyDestroyedDecrements);
		HeatParameters::Parse(parser, "Chasers:Backup",       transitionTriggersBackups);
		HeatParameters::Parse(parser, "Chasers:Clearance",    chaserSpawnClearances);

		HeatParameters::Parse(parser, "Traffic:Independence", trafficIgnoresChasers, trafficIgnoresRoadblocks);

		HeatParameters::Parse(parser, "Joining:Limit", roadblockJoinLimits);

		// Code modifications 
		MemoryTools::Write<byte>(0x00, {0x433CB2}); // min. displayed count
		MemoryTools::Write<byte>(0x90, {0x4443E4}); // roadblock increment

		MemoryTools::MakeRangeNOP(0x4442AC, 0x4442C2); // zero-wave / capacity increment
		MemoryTools::MakeRangeNOP(0x57B186, 0x57B189); // helicopter increment
		MemoryTools::MakeRangeNOP(0x42B74E, 0x42B771); // cops-lost increment
		MemoryTools::MakeRangeNOP(0x4440D7, 0x4440DF); // membership check

		MemoryTools::MakeRangeJMP(ChasersManager::GetNameOfNewChaser, 0x42BA50, 0x42BCEE);
		
		MemoryTools::MakeRangeJMP(WaveReset,          waveResetEntrance,          waveResetExit);
		MemoryTools::MakeRangeJMP(PatrolSpawn,        patrolSpawnEntrance,        patrolSpawnExit);
		MemoryTools::MakeRangeJMP(CopClearance,       copClearanceEntrance,       copClearanceExit);
		MemoryTools::MakeRangeJMP(CopSpawnLimit,      copSpawnLimitEntrance,      copSpawnLimitExit);
		MemoryTools::MakeRangeJMP(ScriptedSpawn,      scriptedSpawnEntrance,      scriptedSpawnExit);
		MemoryTools::MakeRangeJMP(PatrolPursuit,      patrolPursuitEntrance,      patrolPursuitExit);
		MemoryTools::MakeRangeJMP(PatrolDespawn,      patrolDespawnEntrance,      patrolDespawnExit);
		MemoryTools::MakeRangeJMP(RoadblockSpawn,     roadblockSpawnEntrance,     roadblockSpawnExit);
		MemoryTools::MakeRangeJMP(TrafficDensity,     trafficDensityEntrance,     trafficDensityExit);
		MemoryTools::MakeRangeJMP(CopConstructor,     copConstructorEntrance,     copConstructorExit);
		MemoryTools::MakeRangeJMP(RecyclingCheck,     recyclingCheckEntrance,     recyclingCheckExit);
		MemoryTools::MakeRangeJMP(ByClassRequest,     byClassRequestEntrance,     byClassRequestExit);
		MemoryTools::MakeRangeJMP(ScriptedRequest,    scriptedRequestEntrance,    scriptedRequestExit);
		MemoryTools::MakeRangeJMP(ScriptedSpawnReset, scriptedSpawnResetEntrance, scriptedSpawnResetExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [SPA] CopSpawnOverrides");

		activeChaserCounts       .Log("activeChaserCount       ");
		chasersAreIndependents   .Log("chasersAreIndependent   ");
		onlyDestroyedDecrements  .Log("onlyDestroyedDecrement  ");
		transitionTriggersBackups.Log("transitionTriggersBackup");
		chaserSpawnClearances    .Log("chaserSpawnClearance    ");

		trafficIgnoresChasers   .Log("trafficIgnoresChasers   ");
		trafficIgnoresRoadblocks.Log("trafficIgnoresRoadblocks");

		roadblockJoinLimits.Log("roadblockJoinLimit      ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		if (isRacing)
			skipScriptedSpawns = false;

		patrolSpawns   .UpdateSpawnTable();
		scriptedSpawns .UpdateSpawnTable();
		roadblockSpawns.UpdateSpawnTable();

		activeChaserCounts       .SetToHeat(isRacing, heatLevel);
		chasersAreIndependents   .SetToHeat(isRacing, heatLevel);
		onlyDestroyedDecrements  .SetToHeat(isRacing, heatLevel);
		transitionTriggersBackups.SetToHeat(isRacing, heatLevel);
		chaserSpawnClearances    .SetToHeat(isRacing, heatLevel);

		squaredChaserSpawnClearance = chaserSpawnClearances.current * chaserSpawnClearances.current;

		trafficIgnoresChasers   .SetToHeat(isRacing, heatLevel);
		trafficIgnoresRoadblocks.SetToHeat(isRacing, heatLevel);

		roadblockJoinLimits.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		skipScriptedSpawns = true;

		patrolSpawns   .ClearVehicles();
		scriptedSpawns .ClearVehicles();
		roadblockSpawns.ClearVehicles();
	}
}