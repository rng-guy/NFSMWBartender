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
					Globals::logger.LogLongIndent('+', this, "Contingent");
			}
		}


		~Contingent()
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.LogLongIndent('-', this, "Contingent");
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


		void AddVehicle(const address copVehicle)
		{
			const vault copType          = Globals::GetVehicleType(copVehicle);
			const auto  [pair, wasAdded] = this->copTypeToCurrentCount.try_emplace(copType, 1);

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


		bool RemoveVehicle(const address copVehicle)
		{
			const vault copType   = Globals::GetVehicleType(copVehicle);
			const auto  foundType = this->copTypeToCurrentCount.find(copType);

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

	// Heat levels
	HeatParameters::Interval<int>   activeChaserCounts       (1, 8);  // cars
	HeatParameters::Pair    <bool>  chasersAreIndependents   (false);
	HeatParameters::Pair    <bool>  onlyDestroyedDecrements  (false);
	HeatParameters::Pair    <bool>  transitionTriggersBackups(false);
	HeatParameters::Pair    <float> chaserSpawnClearances    (40.f);  // metres

	HeatParameters::Pair<bool> trafficIgnoresChasers   (false);
	HeatParameters::Pair<bool> trafficIgnoresRoadblocks(false);

	HeatParameters::OptionalPair<int> roadblockJoinLimits; // cars

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

		int&   backupFlag  = *reinterpret_cast<int*>  (this->pursuit + 0x218);
		float& backupTimer = *reinterpret_cast<float*>(this->pursuit + 0x21C);

		int& fullWaveCapacity       = *reinterpret_cast<int*>(this->pursuit + 0x144);
		int& numCopsLostInWave      = *reinterpret_cast<int*>(this->pursuit + 0x14C);
		int& numCopsToTriggerBackup = *reinterpret_cast<int*>(this->pursuit + 0x148);

		const bool&  isPerpBusted      = *reinterpret_cast<bool*> (this->pursuit + 0xE8);
		const bool&  bailingPursuit    = *reinterpret_cast<bool*> (this->pursuit + 0xE9);
		const bool&  isFreeRoamPursuit = *reinterpret_cast<bool*> (this->pursuit + 0xA8);
		const float& copSpawnCooldown  = *reinterpret_cast<float*>(this->pursuit + 0xCC);

		Contingent chaserSpawns = Contingent(CopSpawnTables::chaserSpawnTables.current, this->pursuit);

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
			const address attribute = this->GetFromPursuitlevel(0x24F7A1BC); // fetches "NumPatrolCars"

			this->maxNumPatrolCars = (attribute) ? *reinterpret_cast<int*>(attribute) : 0;

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

			if (this->IsSearchModeActive())
				return (numActiveChasers < this->maxNumPatrolCars);

			return ((numActiveChasers < activeChaserCounts.minValues.current) or (this->GetWaveCapacity() > 0));
		}


		void ForceTriggerBackup() const
		{
			static const auto LockInPursuitAttributes = reinterpret_cast<void (__thiscall*)(address)>(0x40A9B0);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SPA] Force-triggering backup");

			this->backupFlag  = 0;
			this->backupTimer = 0.f;

			LockInPursuitAttributes(this->pursuit);
		}


		static bool HasVehicleEngaged(const address copVehicle)
		{
			const address pursuitVehicle = ChasersManager::GetPursuitVehicle(copVehicle);
			return (pursuitVehicle) ? *reinterpret_cast<bool*>(pursuitVehicle + 0x22) : false;
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
				Globals::logger.LogLongIndent('+', this, "ChasersManager");

			this->pursuitToManager.try_emplace(this->pursuit, this);
		}


		~ChasersManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "ChasersManager");

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


		static bool __fastcall GetChaserSpawnStatus(const address pursuit)
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
		static const address& raceStatusObject = *reinterpret_cast<address*>(0x91E000);
		return (raceStatusObject and *reinterpret_cast<int*>(raceStatusObject + 0x1960));
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

	__declspec(naked) void WaveReset()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0x14C], 0x0

			mov ecx, esi
			call ChasersManager::NotifyOfWaveReset // ecx: pursuit

			jmp dword ptr waveResetExit
		}
	}



	constexpr address copRequestEntrance = 0x42BA50;
	constexpr address copRequestExit     = 0x42BCED;

	__declspec(naked) void CopRequest()
	{
		__asm
		{
			call ChasersManager::GetNameOfNewChaser // ecx: pursuit

			jmp dword ptr copRequestExit
		}
	}



	constexpr address patrolSpawnEntrance = 0x430E37;
	constexpr address patrolSpawnExit     = 0x430E3D;

	__declspec(naked) void PatrolSpawn()
	{
		__asm
		{
			push edi // copVehicle
			mov ecx, offset patrolSpawns
			call Contingent::AddVehicle

			// Execute original code and resume
			inc dword ptr [ebp + 0x94]

			jmp dword ptr patrolSpawnExit
		}
	}



	constexpr address copClearanceEntrance = 0x41A139;
	constexpr address copClearanceExit     = 0x41A13F;

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

			mov ecx, dword ptr [esi + 0x54]
			mov byte ptr [ecx - 0x4C + 0x76B], al

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x314]

			jmp dword ptr scriptedSpawnExit
		}
	}



	constexpr address patrolPursuitEntrance = 0x4224B0;
	constexpr address patrolPursuitExit     = 0x4224B6;

	__declspec(naked) void PatrolPursuit()
	{
		__asm
		{
			cmp eax, 0x9AF1BF40 // AIGoalPatrol
			jne conclusion      // not patrol goal

			mov eax, dword ptr [edi + 0x4C - 0x4]
			cmp dword ptr [eax + 0x94], 0x2
			jne conclusion // not cop

			cmp byte ptr [edi + 0x76B], 0x1
			je conclusion // "Scripted" cop

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

	__declspec(naked) void PatrolDespawn()
	{
		__asm
		{
			cmp dword ptr [esi + 0x78], 0x9AF1BF40 // AIGoalPatrol
			jne conclusion                         // not patrol goal

			mov eax, dword ptr [esi - 0x4]
			cmp dword ptr [eax + 0x94], 0x2
			jne conclusion // not cop

			cmp byte ptr [esi - 0x4C + 0x76B], 0x1
			je conclusion // "Scripted" cop

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

	__declspec(naked) void RoadblockSpawn()
	{
		static constexpr address addVehicleToRoadblock = 0x43C4E0;
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
			call dword ptr [edx + 0x80]

			lea eax, dword ptr [esp + 0x1C]
			push eax
			lea ecx, dword ptr [esp + 0x48]
			call dword ptr addVehicleToRoadblock

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

	__declspec(naked) void TrafficDensity()
	{
		__asm
		{
			cmp byte ptr trafficIgnoresChasers.current, 0x1
			je roadblock // "Chasers" ignored

			cmp byte ptr chasersAreIndependents.current, 0x1
			je chasers // "Chasers" independent

			mov eax, dword ptr [ebx - 0x54 + 0x94]
			cmp eax, dword ptr activeChaserCounts.maxValues.current
			jge roadblock // at or above spawn limit

			chasers:
			mov ecx, edi
			call ChasersManager::GetChaserSpawnStatus // ecx: pursuit
			test al, al
			jne conclusion // pending "Chasers" spawn

			roadblock:
			cmp byte ptr trafficIgnoresRoadblocks.current, 0x1
			je conclusion // roadblocks ignored

			cmp byte ptr [edi + 0x190], 0x0

			conclusion:
			jmp dword ptr trafficDensityExit
		}
	}



	constexpr address copConstructorEntrance = 0x41EE72;
	constexpr address copConstructorExit     = 0x41EE7C;

	__declspec(naked) void CopConstructor()
	{
		__asm
		{
			call IsEventActive
			mov byte ptr [esi + 0xA9], al

			// Execute original code and resume
			mov eax, dword ptr [esi + 0x54]

			jmp dword ptr copConstructorExit
		}
	}



	constexpr address recyclingCheckEntrance = 0x41ED5D;
	constexpr address recyclingCheckExit     = 0x41ED66;

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
			cmp al, byte ptr [esi + 0xA9]

			conclusion:
			jmp dword ptr recyclingCheckExit
		}
	}



	constexpr address byNameInterceptorEntrance = 0x41ECEC;
	constexpr address byNameInterceptorExit     = 0x41ECF2;

	__declspec(naked) void ByNameInterceptor()
	{
		__asm
		{
			// Execute original code first
			mov ebp, dword ptr [esp + 0x74]

			cmp dword ptr [esp + 0x70], 0x42E72E
			jne conclusion // not "Scripted" spawn

			cmp byte ptr skipScriptedSpawns, 0x0
			jne conclusion // skip "Scripted" spawn

			cmp byte ptr Globals::playerHeatLevelKnown, 0x1
			jne conclusion // Heat level unknown

			mov ecx, offset scriptedSpawns
			call Contingent::GetNameOfSpawnableCop
			mov ebp, eax

			conclusion:
			test ebp, ebp

			jmp dword ptr byNameInterceptorExit
		}
	}



	constexpr address scriptedSpawnResetEntrance = 0x42E901;
	constexpr address scriptedSpawnResetExit     = 0x42E906;

	__declspec(naked) void ScriptedSpawnReset()
	{
		__asm
		{
			cmp dword ptr [esi + 0x70], 0x1
			jg conclusion // "Scripted" spawns remain

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



	constexpr address byClassInterceptorEntrance = 0x426621;
	constexpr address byClassInterceptorExit     = 0x42672E;

	__declspec(naked) void ByClassInterceptor()
	{
		static constexpr address getPursuitVehicleByName = 0x41ECD0;

		__asm
		{
			je failure // spawn intended to fail

			mov ecx, dword ptr [esp + 0x8]
			call GetByClassReplacement // ecx: caller
			test eax, eax
			je conclusion              // skip this spawn

			mov ecx, ebp
			push eax
			call dword ptr getPursuitVehicleByName
			jmp conclusion // cop replaced

			failure:
			xor eax, eax

			conclusion:
			jmp dword ptr byClassInterceptorExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "CarSpawns.ini");

		// Pursuit-board tracking
		const std::string section = "Board:Tracking";

		parser.ParseParameter<bool>(section, "heavyVehicles",  trackHeavyVehicles);
		parser.ParseParameter<bool>(section, "leaderVehicles", trackLeaderVehicles);
		parser.ParseParameter<bool>(section, "joinedVehicles", trackJoinedVehicles);

		// Heat parameters
		HeatParameters::Parse<int>  (parser, "Chasers:Limits",       {activeChaserCounts,    0});
		HeatParameters::Parse<bool> (parser, "Chasers:Independence", {chasersAreIndependents});
		HeatParameters::Parse<bool> (parser, "Chasers:Decrement",    {onlyDestroyedDecrements});
		HeatParameters::Parse<bool> (parser, "Chasers:Backup",       {transitionTriggersBackups});
		HeatParameters::Parse<float>(parser, "Chasers:Clearance",    {chaserSpawnClearances, 0.f});

		HeatParameters::Parse<bool, bool>(parser, "Traffic:Independence", {trafficIgnoresChasers}, {trafficIgnoresRoadblocks});

		HeatParameters::ParseOptional<int>(parser, "Joining:Limit", {roadblockJoinLimits, 0});

		// Code modifications 
		MemoryTools::Write<byte>(0x00, {0x433CB2}); // min. displayed count
		MemoryTools::Write<byte>(0x90, {0x4443E4}); // roadblock increment

		MemoryTools::MakeRangeNOP(0x4442AC, 0x4442C2); // zero-wave / capacity increment
		MemoryTools::MakeRangeNOP(0x57B186, 0x57B189); // helicopter increment
		MemoryTools::MakeRangeNOP(0x42B74E, 0x42B771); // cops-lost increment
		MemoryTools::MakeRangeNOP(0x4440D7, 0x4440DF); // membership check
		
		MemoryTools::MakeRangeJMP(WaveReset,          waveResetEntrance,          waveResetExit);
		MemoryTools::MakeRangeJMP(CopRequest,         copRequestEntrance,         copRequestExit);
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
		MemoryTools::MakeRangeJMP(ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);
		MemoryTools::MakeRangeJMP(ScriptedSpawnReset, scriptedSpawnResetEntrance, scriptedSpawnResetExit);
		MemoryTools::MakeRangeJMP(ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);
		
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



	void LogSetupReport()
	{
		if (not featureEnabled) return;

		if (trackHeavyVehicles or trackLeaderVehicles or trackJoinedVehicles)
			Globals::logger.Log("  CONFIG [SPA] CopSpawnOverrides");

		if (trackHeavyVehicles)  Globals::logger.LogLongIndent("tracking HeavyStrategy vehicles");
		if (trackLeaderVehicles) Globals::logger.LogLongIndent("tracking LeaderStrategy vehicles");
		if (trackJoinedVehicles) Globals::logger.LogLongIndent("tracking roadblock vehicles");
	}
}