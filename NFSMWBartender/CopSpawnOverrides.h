#pragma once

#include <Windows.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <array>

#include "Globals.h"
#include "PursuitFeatures.h"
#include "CopSpawnTables.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace CopSpawnOverrides
{

	// GlobalSpawnManager class ---------------------------------------------------------------------------------------------------------------------

	class GlobalSpawnManager
	{
	private:

		std::unordered_map<hash, int> copTypeToCurrentCount;

		CopSpawnTables::SpawnTable               spawnTable;
		const CopSpawnTables::SpawnTable** const sourceSpawnTable;



	public:

		GlobalSpawnManager(const CopSpawnTables::SpawnTable** const sourceSpawnTable) : sourceSpawnTable(sourceSpawnTable) {}


		void ReloadSpawnTable()
		{
			if (*(this->sourceSpawnTable))
				this->spawnTable = *(*(this->sourceSpawnTable));

			else if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [GLO] Failed to reload spawn table");

			for (const auto& pair : this->copTypeToCurrentCount)
				this->spawnTable.UpdateCapacity(pair.first, -pair.second);
		}


		void ResetSpawnTable()
		{
			for (const auto& pair : this->copTypeToCurrentCount)
				this->spawnTable.UpdateCapacity(pair.first, pair.second);

			this->copTypeToCurrentCount.clear();
		}


		const char* GetRandomCopName()
		{
			const char* copName = this->spawnTable.GetRandomCopName();

			if ((not copName) and *(this->sourceSpawnTable))
				copName = (*(*(this->sourceSpawnTable))).GetRandomCopName();

			return copName;
		}


		void NotifyOfSpawn(const address copVehicle)
		{
			static hash (__thiscall* const GetCopType)(address) = (hash (__thiscall*)(address))0x6880A0;

			const hash copType = GetCopType(copVehicle);
			if (not copType) return;

			const auto foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType == this->copTypeToCurrentCount.end())
				this->copTypeToCurrentCount.insert({copType, 1});

			else (foundType->second)++;

			this->spawnTable.UpdateCapacity(copType, -1);
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	int minActiveCount = 0; // vehicles
	int maxActiveCount = 8; // vehicles

	// General Heat levels
	std::array<int, Globals::maxHeatLevel> minActiveCounts = {};
	std::array<int, Globals::maxHeatLevel> maxActiveCounts = {};

	// Code caves
	bool skipEventSpawns = true;

	const address addVehicleToRoadblock   = 0x43C4E0;
	const address getPursuitVehicleByName = 0x41ECD0;
	const address stringToHashFunction    = 0x5CC240;
	const address copTableComparison      = 0x90D8C8;

	std::unique_ptr<GlobalSpawnManager> eventManager     = nullptr;
	std::unique_ptr<GlobalSpawnManager> roadblockManager = nullptr;





	// ContingentManager class ----------------------------------------------------------------------------------------------------------------------

	class ContingentManager : public PursuitFeatures::CopVehicleReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:

		const address pursuit;

		int  numPatrolCarsToSpawn = 0;
		int  numCopsInContingent  = 0;

		int* const fullWaveCapacity  = (int*)(pursuit + 0x144);
		int* const numCopsLostInWave = (int*)(pursuit + 0x14C);

		CopSpawnTables::SpawnTable    spawnTable;
		std::unordered_map<hash, int> copTypeToCurrentCount;

		inline static std::unordered_map<address, ContingentManager*> pursuitToManager;


		void UpdateSpawnTable()
		{
			if (not this->IsHeatLevelKnown())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log("WARNING: [SPA] Failed table update in", this->pursuit);

				return;
			}

			this->spawnTable = *CopSpawnTables::pursuitSpawnTable;

			for (const auto& pair : this->copTypeToCurrentCount)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[SPA] Table update:", pair.first, -(pair.second));

				this->spawnTable.UpdateCapacity(pair.first, -(pair.second));

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[SPA] Type capacity remaining:", this->spawnTable.GetCapacity(pair.first));
			}
		}


		void UpdateNumPatrolCars()
		{
			static constexpr key patrolCarsAttribute = 0x24F7A1BC;

			// Cannot be called in the constructor or immediately after a Heat transition, as Vault values update slightly later
			static address (__thiscall* const GetPursuitAttributes)(address)   = (address (__thiscall*)(address))0x418E90;
			static address (__thiscall* const GetAttribute)(address, key, int) = (address (__thiscall*)(address, key, int))0x454810;

			const address numPatrolCars = GetAttribute(GetPursuitAttributes(this->pursuit), patrolCarsAttribute, 0);
			this->numPatrolCarsToSpawn  = (numPatrolCars) ? *(int*)numPatrolCars : 0;

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[SPA] Patrol cars:", this->numPatrolCarsToSpawn);
		}


		bool IsSearchModeActive() const 
		{
			return (*(int*)(this->pursuit + 0x218) == 2);
		}


		int GetWaveCapacity() const
		{
			return *(this->fullWaveCapacity) - (*(this->numCopsLostInWave) + this->numCopsInContingent);
		}


		void CorrectWaveCapacity() const
		{
			if (*(this->fullWaveCapacity) == 0) return;

			const int waveCapacity = this->GetWaveCapacity();

			if (waveCapacity < 0)
			{
				*(this->fullWaveCapacity) -= waveCapacity;

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[SPA] Wave correction:", -waveCapacity);
			}
		}


		bool IsWaveExhausted() const
		{
			const int minCount = (this->IsSearchModeActive()) ? this->numPatrolCarsToSpawn : minActiveCount;
			return (not ((this->GetWaveCapacity() > 0) or (this->numCopsInContingent < minCount)));
		}



	public:

		static const ContingentManager* GetInstance(const address pursuit)
		{
			const auto foundPursuit = ContingentManager::pursuitToManager.find(pursuit);
			return (foundPursuit != ContingentManager::pursuitToManager.end()) ? foundPursuit->second : nullptr;
		}


		ContingentManager(const address pursuit) 
			: pursuit(pursuit)
		{
			this->pursuitToManager.insert({this->pursuit, this});
			this->UpdateSpawnTable();
		}


		~ContingentManager() override 
		{
			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnGameplay() override 
		{		
			this->CorrectWaveCapacity();
		}


		void UpdateOnHeatChange() override 
		{
			this->UpdateSpawnTable();
		}


		void UpdateOncePerHeatLevel() override
		{
			this->UpdateNumPatrolCars();
		}


		void ProcessAddition
		(
			const address  copVehicle,
			const hash     copType,
			const CopLabel copLabel
		) 
			override 
		{
			if (skipEventSpawns) skipEventSpawns = false;
			if (copLabel != CopLabel::CHASER) return;

			const auto foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType == this->copTypeToCurrentCount.end())
				this->copTypeToCurrentCount.insert({copType, 1});
			
			else (foundType->second)++;

			this->spawnTable.UpdateCapacity(copType, -1);
			(this->numCopsInContingent)++;

			if constexpr (Globals::loggingEnabled)
			{
				Globals::Log(this->pursuit, "[SPA] Type capacity remaining:", this->spawnTable.GetCapacity(copType));
				Globals::Log(this->pursuit, "[SPA] Contingent:",              this->numCopsInContingent);
			}
		}


		void ProcessRemoval
		(
			const address  copVehicle,
			const address  copType,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel != CopLabel::CHASER) return;

			const auto foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType != this->copTypeToCurrentCount.end())
			{
				if (--(foundType->second) == 0)
					this->copTypeToCurrentCount.erase(foundType);

				this->spawnTable.UpdateCapacity(copType, +1);
				(this->numCopsInContingent)--;

				(*(this->numCopsLostInWave))++;

				if constexpr (Globals::loggingEnabled)
				{
					Globals::Log(this->pursuit, "[SPA] Type capacity remaining:", this->spawnTable.GetCapacity(copType));
					Globals::Log(this->pursuit, "[SPA] Contingent:", this->numCopsInContingent);
				}
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [SPA] Unknown type", copType, "in", this->pursuit);
		}


		const char* GetRandomCopName() const
		{		
			return (this->IsWaveExhausted()) ? nullptr : this->spawnTable.GetRandomCopName();
		}
	};





	// Auxiliary functions -----------------------------------------------------------------------------------------------------------------------------

	void __fastcall NotifyRoadblockManager(const address copVehicle)
	{
		roadblockManager.get()->NotifyOfSpawn(copVehicle);
	}



	void ResetRoadblockManager()
	{
		roadblockManager.get()->ResetSpawnTable();
	}



	void __fastcall NotifyEventManager(const address copVehicle)
	{
		eventManager.get()->NotifyOfSpawn(copVehicle);
	}



	const char* __fastcall GetByClassReplacement(const address spawnReturn)
	{
		if (ContingentManager::IsHeatLevelKnown())
		{
			switch (spawnReturn)
			{
			case 0x4269E6: // helicopter
				return CopSpawnTables::helicopterVehicle;

			case 0x430DAD: // free-roam patrol
				return CopSpawnTables::patrolSpawnTable->GetRandomCopName();
				
			case 0x42EAAD: // first cop of milestone / bounty pursuit
				return CopSpawnTables::pursuitSpawnTable->GetRandomCopName();
				
			case 0x43E049: // roadblock
				return roadblockManager.get()->GetRandomCopName();

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::Log("WARNING: [SPA] Unknown ByClass caller:", spawnReturn);
			}
		}

		return nullptr;
	}



	bool __cdecl IsByNameReplacementAvailable
	(
		const address spawnReturn,
		const address pursuit,
		const char**  newCopName
	) {
		if (ContingentManager::IsHeatLevelKnown())
		{
			switch (spawnReturn)
			{
			case 0x42E72E: // scripted event spawn
				if (skipEventSpawns) break;
				*newCopName = eventManager.get()->GetRandomCopName();
				return true;
				
			case 0x43EBD0: // pursuit spawn, Cooldown mode patrol
				const ContingentManager* const manager = ContingentManager::GetInstance(pursuit);
				*newCopName = (manager) ? manager->GetRandomCopName() : nullptr;
				return manager;
			}
		}

		*newCopName = nullptr;
		return false;
	}





	// Code caves --------------------------------------------------------------------------------------------------------------------------------------

	const address roadblockSpawnEntrance = 0x43E04F;
	const address roadblockSpawnExit     = 0x43E06C;
	const address roadblockSpawnSkip     = 0x43E031;

	__declspec(naked) void RoadblockSpawn()
	{
		__asm
		{
			je conclusion // spawn intended to fail

			mov ecx, eax

			push ecx
			call NotifyRoadblockManager // ecx: PVehicle
			pop ecx

			mov edx, [ecx]
			call dword ptr [edx + 0x80]

			lea eax, [esp + 0x1C]
			push eax
			lea ecx, [esp + 0x48]
			call dword ptr addVehicleToRoadblock

			conclusion:
			dec edi
			jne skip // car(s) left to generate

			call ResetRoadblockManager

			jmp dword ptr roadblockSpawnExit

			skip:
			jmp dword ptr roadblockSpawnSkip
		}
	}



	const address byNameInterceptorEntrance = 0x41ECEC;
	const address byNameInterceptorExit     = 0x41ECF2;

	__declspec(naked) void ByNameInterceptor()
	{
		__asm
		{
			// Execute original code first
			mov ebp, [esp + 0x74]
			test ebp, ebp
			je failure // spawn intended to fail

			lea eax, [esp - 0x4] // new const char**

			push eax          // newCopName
			push esi          // AIPursuit (only for "Chasers" calls)
			push [esp + 0x78] // return address
			call IsByNameReplacementAvailable
			add esp, 0x8      // pop return address and AIPursuit

			test al, al
			pop eax       // pop newCopName     
			je conclusion // valid spawn, but do not intercept

			mov ebp, eax
			mov [esp + 0x74], ebp

			conclusion:
			test ebp, ebp

			failure:
			jmp dword ptr byNameInterceptorExit
		}
	}



	const address byClassInterceptorEntrance = 0x426621;
	const address byClassInterceptorExit     = 0x426627;

	__declspec(naked) void ByClassInterceptor()
	{
		__asm
		{
			je failure // spawn intended to fail

			mov ecx, [esp + 0x8]
			call GetByClassReplacement // ecx: return address
			test eax, eax
			je conclusion              // skip this spawn

			mov ecx, ebp                           // AICopManager
			push eax                               // newCopName
			call dword ptr getPursuitVehicleByName // pops newCopName
			jmp conclusion                         // cop replaced

			failure:
			xor eax, eax

			conclusion:
			// Execute original code and resume
			pop ebp
			pop ecx
			jmp dword ptr byClassInterceptorExit
		}
	}



	const address copRefillEntrance = 0x4442AE;
	const address copRefillExit     = 0x4442B6;

	__declspec(naked) void CopRefill()
	{
		__asm
		{
			cmp dword ptr minActiveCount, 0x0
			je conclusion // zero is valid engagement count

			inc edx
			mov [esi + 0x184], edx
			inc eax

			conclusion:
			jmp dword ptr copRefillExit
		}
	}



	const address firstCopTableEntrance = 0x4242F9;
	const address firstCopTableExit     = 0x424323;

	__declspec(naked) void FirstCopTable()
	{
		__asm
		{
			mov edx, 0x0 // DWORD 4
			lea ecx, [ecx + ecx * 0x2]
			mov [ebp + ecx * 0x8 + 0xC], edx

			mov edx, 0x0 // DWORD 1
			lea ecx, [ebp + ecx * 0x8 + 0x0]
			mov [ecx], edx

			mov edx, 0x0 // DWORD 2
			mov [ecx + 0x4], edx

			mov edx, 0x0 // DWORD 3
			mov [ecx + 0x8], edx

			mov edx, CopSpawnTables::currentMaxCopCapacity // COUNT
			mov [ecx + 0x10], edx

			mov edx, 0x1 // CHANCE
			mov [ecx + 0x14], edx

			jmp dword ptr firstCopTableExit
		}
	}



	const address secondCopTableEntrance = 0x424205;
	const address secondCopTableExit     = 0x42420E;

	__declspec(naked) void SecondCopTable()
	{
		__asm
		{
			mov ecx, 0x0 // DWORD 3

			push eax
			mov eax, copTableComparison
			mov eax, [eax]
			cmp ecx, eax
			pop eax

			jmp dword ptr secondCopTableExit
		}
	}



	const address thirdCopTableEntrance = 0x424210;
	const address thirdCopTableExit     = 0x424217;

	__declspec(naked) void ThirdCopTable()
	{
		__asm
		{
			mov edx, CopSpawnTables::currentMaxCopCapacity // COUNT
			add [esp + 0x28], edx

			jmp dword ptr thirdCopTableExit
		}
	}



	const address fourthCopTableEntrance = 0x424298;
	const address fourthCopTableExit     = 0x4242A1;

	__declspec(naked) void FourthCopTable()
	{
		__asm
		{
			mov ecx, 0x0 // DWORD 3

			push eax
			mov eax, copTableComparison
			mov eax, [eax]
			cmp ecx, eax
			pop eax

			jmp dword ptr fourthCopTableExit
		}
	}



	const address fifthCopTableEntrance = 0x4242D2;
	const address fifthCopTableExit     = 0x4242D7;

	__declspec(naked) void FifthCopTable()
	{
		__asm
		{
			mov ebx, CopSpawnTables::currentMaxCopCapacity // COUNT
			mov eax, ebx

			jmp dword ptr fifthCopTableExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		parser.LoadFile(Globals::configAdvancedPath + "Cars.ini");

		parser.ParseParameterTable
		(
			"Spawning:Limits",
			Globals::configFormat,
			ConfigParser::FormatParameter<int, Globals::maxHeatLevel>(minActiveCounts, minActiveCount, 0),
			ConfigParser::FormatParameter<int, Globals::maxHeatLevel>(maxActiveCounts, maxActiveCount, 0)
		);

		MemoryEditor::WriteToByteRange(0x90, 0x4442BC, 6); // wave-capacity increment
		MemoryEditor::WriteToByteRange(0x90, 0x42B76B, 6); // cops-lost increment

		MemoryEditor::Write<BYTE>(0x00, 0x433CB2);            // min. displayed count
		MemoryEditor::Write<BYTE>(0x90, 0x57B188,  0x4443E4); // helicopter / roadblock increment
		MemoryEditor::Write<BYTE>(0xEB, 0x42B9AA,  0x44389E); // foreign / HeavyStrategy cops fleeing

		MemoryEditor::DigCodeCave(&RoadblockSpawn,     roadblockSpawnEntrance,     roadblockSpawnExit);
		MemoryEditor::DigCodeCave(&ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);
		MemoryEditor::DigCodeCave(&ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);

		MemoryEditor::DigCodeCave(&CopRefill,       copRefillEntrance,       copRefillExit);
		MemoryEditor::DigCodeCave(&FirstCopTable,   firstCopTableEntrance,   firstCopTableExit);
		MemoryEditor::DigCodeCave(&SecondCopTable,  secondCopTableEntrance,  secondCopTableExit);
		MemoryEditor::DigCodeCave(&ThirdCopTable,   thirdCopTableEntrance,   thirdCopTableExit);
		MemoryEditor::DigCodeCave(&FourthCopTable,  fourthCopTableEntrance,  fourthCopTableExit);
		MemoryEditor::DigCodeCave(&FifthCopTable,   fifthCopTableEntrance,   fifthCopTableExit);

		eventManager     = std::make_unique<GlobalSpawnManager>(&(CopSpawnTables::eventSpawnTable));
		roadblockManager = std::make_unique<GlobalSpawnManager>(&(CopSpawnTables::roadblockSpawnTable));

		featureEnabled = true;
	}



	void SetToHeat(size_t heatLevel)
	{
		if (not featureEnabled) return;

		minActiveCount = minActiveCounts[heatLevel - 1];
		maxActiveCount = maxActiveCounts[heatLevel - 1];

		eventManager.get()->ReloadSpawnTable();
		roadblockManager.get()->ReloadSpawnTable();
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		skipEventSpawns = true;

		eventManager.get()->ResetSpawnTable();
		roadblockManager.get()->ResetSpawnTable();
	}
}