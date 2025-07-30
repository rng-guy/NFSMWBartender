#pragma once

#include <Windows.h>
#include <unordered_map>
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
				Globals::Log("WARNING: [GLO] Failed table reload");

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

	// Free-roam Heat levels
	std::array<int, Globals::maxHeatLevel> roamMinActiveCounts = {};
	std::array<int, Globals::maxHeatLevel> roamMaxActiveCounts = {};

	// Racing Heat levels
	std::array<int, Globals::maxHeatLevel> raceMinActiveCounts = {};
	std::array<int, Globals::maxHeatLevel> raceMaxActiveCounts = {};

	// Code caves
	bool skipEventSpawns = true;

	constexpr address addVehicleToRoadblock   = 0x43C4E0;
	constexpr address getPursuitVehicleByName = 0x41ECD0;
	constexpr address stringToHashFunction    = 0x5CC240;
	constexpr address copTableComparison      = 0x90D8C8;

	GlobalSpawnManager eventManager(&(CopSpawnTables::eventSpawnTable));
	GlobalSpawnManager roadblockManager(&(CopSpawnTables::roadblockSpawnTable));





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
					Globals::Log(this->pursuit, "[SPA] Updating table: skipped");

				return;
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[SPA] Updating table: success");

			this->spawnTable = *(CopSpawnTables::pursuitSpawnTable);

			for (const auto& pair : this->copTypeToCurrentCount)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[SPA] Initialising table:", pair.first, -(pair.second));

				this->spawnTable.UpdateCapacity(pair.first, -(pair.second));

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[SPA] Type capacity:", this->spawnTable.GetCapacity(pair.first));
			}
		}


		void UpdateNumPatrolCars()
		{
			static constexpr key patrolCarsAttribute = 0x24F7A1BC;

			// Cannot be called in the constructor or immediately after a Heat transition, as Vault values update slightly later
			static address (__thiscall* const GetPursuitAttributes)(address)   = (address (__thiscall*)(address))0x418E90;
			static address (__thiscall* const GetAttribute)(address, key, int) = (address (__thiscall*)(address, key, int))0x454810;

			const address numPatrolCars = GetAttribute(GetPursuitAttributes(this->pursuit), patrolCarsAttribute, 0);
			this->numPatrolCarsToSpawn = (numPatrolCars) ? *((int*)numPatrolCars) : 0;

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[SPA] Patrol cars:", this->numPatrolCarsToSpawn);
		}


		bool IsSearchModeActive() const 
		{
			return (*((int*)(this->pursuit + 0x218)) == 2);
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
				Globals::Log(this->pursuit, "[SPA] Type capacity:", this->spawnTable.GetCapacity(copType));
				Globals::Log(this->pursuit, "[SPA] Contingent:",    this->numCopsInContingent);
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
					Globals::Log(this->pursuit, "[SPA] Type capacity:", this->spawnTable.GetCapacity(copType));
					Globals::Log(this->pursuit, "[SPA] Contingent:",    this->numCopsInContingent);
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

	void __fastcall NotifyEventManager(const address copVehicle)
	{
		eventManager.NotifyOfSpawn(copVehicle);
	}



	void ResetEventManager()
	{
		eventManager.ResetSpawnTable();
	}



	void __fastcall NotifyRoadblockManager(const address copVehicle)
	{
		roadblockManager.NotifyOfSpawn(copVehicle);
	}



	void ResetRoadblockManager()
	{
		roadblockManager.ResetSpawnTable();
	}



	const char* __fastcall GetByClassReplacement(const address spawnReturn)
	{
		if (ContingentManager::IsHeatLevelKnown())
		{
			switch (spawnReturn)
			{
			case 0x4269E6: // helicopter
				return CopSpawnTables::helicopterVehicle;

			case 0x42EAAD: // first cop of milestone / bounty pursuit
				[[fallthrough]];

			case 0x430DAD: // free-roam patrol
				return CopSpawnTables::patrolSpawnTable->GetRandomCopName();
				
			case 0x43E049: // roadblock
				return roadblockManager.GetRandomCopName();
			}

			if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [SPA] Unknown ByClass return address:", spawnReturn);
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
				*newCopName = eventManager.GetRandomCopName();
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

	constexpr address roadblockSpawnEntrance = 0x43E04F;
	constexpr address roadblockSpawnExit     = 0x43E06C;
	constexpr address roadblockSpawnSkip     = 0x43E031;

	__declspec(naked) void RoadblockSpawn()
	{
		__asm
		{
			je conclusion // spawn intended to fail

			mov ecx, eax

			push ecx
			call NotifyRoadblockManager // ecx: PVehicle
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

			call ResetRoadblockManager

			jmp dword ptr roadblockSpawnExit

			skip:
			jmp dword ptr roadblockSpawnSkip
		}
	}



	constexpr address eventSpawnResetEntrance = 0x58E7E0;
	constexpr address eventSpawnResetExit     = 0x58E7E6;

	__declspec(naked) void EventSpawnReset()
	{
		__asm
		{
			add esp, 0x4

			push eax
			call ResetEventManager
			pop eax

			mov dword ptr [esp], eax

			jmp dword ptr eventSpawnResetExit
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
			test ebp, ebp
			je failure // spawn intended to fail

			lea eax, dword ptr [esp - 0x4] // new const char**

			push eax                    // newCopName
			push esi                    // AIPursuit (only for "Chasers" calls)
			push dword ptr [esp + 0x78] // return address
			call IsByNameReplacementAvailable
			add esp, 0x8                // pop return address and AIPursuit

			test al, al
			pop eax       // pop newCopName     
			je conclusion // valid spawn, but do not intercept

			mov ebp, eax
			mov dword ptr [esp + 0x74], ebp

			conclusion:
			test ebp, ebp

			failure:
			jmp dword ptr byNameInterceptorExit
		}
	}



	constexpr address byClassInterceptorEntrance = 0x426621;
	constexpr address byClassInterceptorExit     = 0x426627;

	__declspec(naked) void ByClassInterceptor()
	{
		__asm
		{
			je failure // spawn intended to fail

			mov ecx, dword ptr [esp + 0x8]
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



	constexpr address copRefillEntrance = 0x4442AE;
	constexpr address copRefillExit     = 0x4442B6;

	__declspec(naked) void CopRefill()
	{
		__asm
		{
			cmp dword ptr minActiveCount, 0x0
			je conclusion // zero is valid engagement count

			inc edx
			mov dword ptr [esi + 0x184], edx
			inc eax

			conclusion:
			jmp dword ptr copRefillExit
		}
	}



	constexpr address firstCopTableEntrance = 0x4242F9;
	constexpr address firstCopTableExit     = 0x424323;

	__declspec(naked) void FirstCopTable()
	{
		__asm
		{
			xor edx, edx

			lea ecx, dword ptr [ecx + ecx * 0x2]
			mov dword ptr [ebp + ecx * 0x8 + 0xC], edx // DWORD 4

			lea ecx, dword ptr [ebp + ecx * 0x8 + 0x0]
			mov dword ptr [ecx], edx // DWORD 1

			mov dword ptr [ecx + 0x4], edx // DWORD 2
			mov dword ptr [ecx + 0x8], edx // DWORD 3

			mov edx, dword ptr CopSpawnTables::currentMaxCopCapacity // COUNT
			mov dword ptr [ecx + 0x10], edx

			mov edx, 0x1 // CHANCE
			mov dword ptr [ecx + 0x14], edx

			jmp dword ptr firstCopTableExit
		}
	}



	constexpr address secondCopTableEntrance = 0x424205;
	constexpr address secondCopTableExit     = 0x42420E;

	__declspec(naked) void SecondCopTable()
	{
		__asm
		{
			mov eax, dword ptr copTableComparison
			mov eax, dword ptr [eax]
			test eax, eax // DWORD 3

			jmp dword ptr secondCopTableExit
		}
	}



	constexpr address thirdCopTableEntrance = 0x424210;
	constexpr address thirdCopTableExit     = 0x424217;

	__declspec(naked) void ThirdCopTable()
	{
		__asm
		{
			mov edx, dword ptr CopSpawnTables::currentMaxCopCapacity // COUNT
			add dword ptr [esp + 0x28], edx

			jmp dword ptr thirdCopTableExit
		}
	}



	constexpr address fourthCopTableEntrance = 0x424298;
	constexpr address fourthCopTableExit     = 0x4242A1;

	__declspec(naked) void FourthCopTable()
	{
		__asm
		{
			mov eax, dword ptr copTableComparison
			mov eax, dword ptr [eax]
			test eax, eax // DWORD 3

			jmp dword ptr fourthCopTableExit
		}
	}



	constexpr address fifthCopTableEntrance = 0x4242D2;
	constexpr address fifthCopTableExit     = 0x4242D7;

	__declspec(naked) void FifthCopTable()
	{
		__asm
		{
			mov ebx, dword ptr CopSpawnTables::currentMaxCopCapacity // COUNT
			mov eax, ebx

			jmp dword ptr fifthCopTableExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		parser.LoadFile(Globals::configPathAdvanced + "Cars.ini");

		// Pursuit parameters
		Globals::ParseHeatParameterPair<int, int>
		(
			parser,
			"Spawning:Limits",
			roamMinActiveCounts,
			roamMaxActiveCounts,
			raceMinActiveCounts,
			raceMaxActiveCounts,
			minActiveCount,
			maxActiveCount,
			0,
			0
		);

		// Code caves
		MemoryEditor::WriteToByteRange(0x90, 0x4442BC, 6); // wave-capacity increment
		MemoryEditor::WriteToByteRange(0x90, 0x42B76B, 6); // cops-lost increment

		MemoryEditor::Write<BYTE>(0x00, 0x433CB2);           // min. displayed count
		MemoryEditor::Write<BYTE>(0x90, 0x57B188, 0x4443E4); // helicopter / roadblock increment
		MemoryEditor::Write<BYTE>(0xEB, 0x42B9AA, 0x44389E); // foreign / HeavyStrategy cops fleeing

		MemoryEditor::DigCodeCave(&RoadblockSpawn,     roadblockSpawnEntrance,     roadblockSpawnExit);
		MemoryEditor::DigCodeCave(&EventSpawnReset,    eventSpawnResetEntrance,    eventSpawnResetExit);
		MemoryEditor::DigCodeCave(&ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);
		MemoryEditor::DigCodeCave(&ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);

		MemoryEditor::DigCodeCave(&CopRefill,       copRefillEntrance,       copRefillExit);
		MemoryEditor::DigCodeCave(&FirstCopTable,   firstCopTableEntrance,   firstCopTableExit);
		MemoryEditor::DigCodeCave(&SecondCopTable,  secondCopTableEntrance,  secondCopTableExit);
		MemoryEditor::DigCodeCave(&ThirdCopTable,   thirdCopTableEntrance,   thirdCopTableExit);
		MemoryEditor::DigCodeCave(&FourthCopTable,  fourthCopTableEntrance,  fourthCopTableExit);
		MemoryEditor::DigCodeCave(&FifthCopTable,   fifthCopTableEntrance,   fifthCopTableExit);

		return (featureEnabled = true);
	}



	void SetToHeat
	(
		const size_t heatLevel,
		const bool   isRacing
	) {
		if (not featureEnabled) return;

		if (isRacing)
		{
			skipEventSpawns = false;

			minActiveCount = raceMinActiveCounts[heatLevel - 1];
			maxActiveCount = raceMaxActiveCounts[heatLevel - 1];
		}
		else
		{
			minActiveCount = roamMinActiveCounts[heatLevel - 1];
			maxActiveCount = roamMaxActiveCounts[heatLevel - 1];
		}

		eventManager.ReloadSpawnTable();
		roadblockManager.ReloadSpawnTable();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[SPA] CopSpawnOverrides");

			Globals::LogLongIndent("minActiveCount         :", minActiveCount);
			Globals::LogLongIndent("maxActiveCount         :", maxActiveCount);
		}
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		skipEventSpawns = true;

		eventManager.ResetSpawnTable();
		roadblockManager.ResetSpawnTable();
	}
}