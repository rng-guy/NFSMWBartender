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

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = true;

	// Current Heat level
	int minActiveCount = 0; // vehicles
	int maxActiveCount = 8; // vehicles

	// General Heat levels
	std::array<int, Globals::maxHeatLevel> minActiveCounts = {};
	std::array<int, Globals::maxHeatLevel> maxActiveCounts = {};

	// Code caves
	const address getPursuitVehicleByName = 0x41ECD0;
	const address stringToHashFunction    = 0x5CC240;
	const address copTableComparison      = 0x90D8C8;





	// SpawnOverride class --------------------------------------------------------------------------------------------------------------------------

	class ContingentManager : public PursuitFeatures::CopVehicleReaction
	{
	private:

		const address pursuit;

		bool patrolUpdatePending  = true;
		int  numPatrolCarsToSpawn = 0;
		int  numCopsInContingent  = 0;

		int* const fullWaveCapacity  = (int*)(pursuit + 0x144);
		int* const numCopsLostInWave = (int*)(pursuit + 0x14C);

		CopSpawnTables::SpawnTable    spawnTable;
		std::unordered_map<hash, int> copTypeToCurrentCount;

		inline static std::unordered_map<address, ContingentManager*> pursuitToManager;


		void UpdateSpawnTable()
		{
			if (not this->IsHeatLevelKnown()) return;
			this->spawnTable = *CopSpawnTables::pursuitSpawnTable;

			for (const auto& pair : this->copTypeToCurrentCount)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "SPA: Table update:", pair.first, -(pair.second));

				this->spawnTable.UpdateCapacity(pair.first, -(pair.second));

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "SPA: Type capacity:", this->spawnTable.GetCapacity(pair.first));
			}
		}


		void UpdateNumPatrolCars()
		{
			if (not this->patrolUpdatePending) return;

			static constexpr key patrolCarsAttribute = 0x24F7A1BC;

			// Cannot be called in the constructor or immediately after a Heat transition, as Vault values update slightly later
			static address (__thiscall* const GetPursuitAttributes)(address)   = (address (__thiscall*)(address))0x418E90;
			static address (__thiscall* const GetAttribute)(address, key, int) = (address (__thiscall*)(address, key, int))0x454810;

			const address numPatrolCars = GetAttribute(GetPursuitAttributes(this->pursuit), patrolCarsAttribute, 0);
			this->numPatrolCarsToSpawn  = (numPatrolCars) ? *(int*)numPatrolCars : 0;
			this->patrolUpdatePending   = false;

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "SPA: Patrol cars:", this->numPatrolCarsToSpawn);
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
					Globals::Log(this->pursuit, "SPA: Wave correction:", -waveCapacity);
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
			this->UpdateNumPatrolCars();
			this->CorrectWaveCapacity();
		}


		void UpdateOnHeatChange() override 
		{
			this->patrolUpdatePending = true;
			this->UpdateSpawnTable();
		}


		void ProcessAddition
		(
			const address                   addedCopVehicle,
			const hash                      copType,
			const PursuitFeatures::CopLabel copLabel
		) 
			override 
		{
			if (copLabel != PursuitFeatures::CopLabel::CHASER) return;

			const auto foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType == this->copTypeToCurrentCount.end())
				this->copTypeToCurrentCount.insert({copType, 1});
			
			else (foundType->second)++;

			this->spawnTable.UpdateCapacity(copType, -1);
			(this->numCopsInContingent)++;

			if constexpr (Globals::loggingEnabled)
			{
				Globals::Log(this->pursuit, "SPA: Type capacity:", this->spawnTable.GetCapacity(copType));
				Globals::Log(this->pursuit, "SPA: Contingent:",    this->numCopsInContingent);
			}
		}


		void ProcessRemoval
		(
			const address                   removedCopVehicle,
			const address                   copType,
			const PursuitFeatures::CopLabel copLabel
		) 
			override
		{
			if (copLabel != PursuitFeatures::CopLabel::CHASER) return;

			if (--(this->copTypeToCurrentCount.at(copType)) == 0)
				this->copTypeToCurrentCount.erase(copType);
			
			this->spawnTable.UpdateCapacity(copType, +1);
			(this->numCopsInContingent)--;

			(*(this->numCopsLostInWave))++;

			if constexpr (Globals::loggingEnabled)
			{
				Globals::Log(this->pursuit, "SPA: Type capacity:", this->spawnTable.GetCapacity(copType));
				Globals::Log(this->pursuit, "SPA: Contingent:",    this->numCopsInContingent);
			}
		}


		const char* GetRandomCopType() const
		{		
			return (this->IsWaveExhausted()) ? nullptr : this->spawnTable.GetRandomCopType();
		}
	};





	// Auxiliary functions -----------------------------------------------------------------------------------------------------------------------------

	struct Action
	{
		bool        replaceCop; // returned in eax
		const char* newCopName; // returned in edx
	};



	Action __cdecl InterceptClassSpawner(const address spawnCaller)
	{
		if (ContingentManager::IsHeatLevelKnown())
		{
			switch (spawnCaller)
			{
			case 0x4269E6: // helicopter
				return {true, CopSpawnTables::helicopterVehicle};

			case 0x430DAD: // free-roam patrols
				return {true, CopSpawnTables::patrolSpawnTable->GetRandomCopType()};

			case 0x42EAAD: // first scripted cop
				return {true, CopSpawnTables::pursuitSpawnTable->GetRandomCopType()};

			case 0x43E049: // roadblocks
				return {true, CopSpawnTables::roadblockSpawnTable->GetRandomCopType()};
			}
		}

		return {false, nullptr};
	}



	Action __cdecl InterceptNameSpawner
	(
		const address spawnCaller,
		const address pursuit
	) {
		if (ContingentManager::IsHeatLevelKnown())
		{
			switch (spawnCaller)
			{
			case 0x42E72E: // event spawns
				return {true, CopSpawnTables::eventSpawnTable->GetRandomCopType()};

			case 0x43EBD0: // pursuit vehicles, cooldown patrols
				const ContingentManager* const manager = ContingentManager::GetInstance(pursuit);
				if (manager) return {true, manager->GetRandomCopType()};
			}
		}

		return {false, nullptr};
	}





	// Code caves --------------------------------------------------------------------------------------------------------------------------------------

	const address byClassInterceptorEntrance = 0x426621;
	const address byClassInterceptorExit     = 0x426627;
	const address byClassInterceptorSkip     = 0x42662A;

	__declspec(naked) void ByClassInterceptor()
	{
		__asm
		{
			je failure // spawn intended to fail

			push [esp + 0x8] // caller address
			call InterceptClassSpawner
			add esp, 0x4

			test al, al
			je skip // valid spawn, but do not intercept

			mov ecx, ebp // AICopManager
			push edx     // copName
			call dword ptr getPursuitVehicleByName
			jmp conclusion // intercepted

			failure:
			xor eax, eax

			conclusion:
			// Execute original code and resume
			pop ebp
			pop ecx
			jmp dword ptr byClassInterceptorExit

			skip:
			jmp dword ptr byClassInterceptorSkip
		}
	}



	const address byNameInterceptorEntrance = 0x41ECEC;
	const address byNameInterceptorExit     = 0x41ECF2;

	__declspec(naked) void ByNameInterceptor()
	{
		__asm
		{
			// Original code
			mov ebp, [esp + 0x74]
			test ebp, ebp
			je conclusion // spawn intended to fail

			push esi          // AIPursuit
			push [esp + 0x74] // caller address
			call InterceptNameSpawner
			add esp, 0x8

			test al, al
			je conclusion // valid spawn, but do not intercept

			mov ebp, edx
			mov [esp + 0x74], ebp // intercepted

			conclusion:
			// Execute original code and resume
			test ebp, ebp

			jmp dword ptr byNameInterceptorExit
		}
	}



	const address copRefillEntrance = 0x4442AE;
	const address copRefillExit     = 0x4442B6;

	__declspec(naked) void CopRefill()
	{
		__asm
		{
			cmp dword ptr minActiveCount, 0x0
			je conclusion

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



	const address otherSpawnLimitEntrance = 0x426C4E;
	const address otherSpawnLimitExit     = 0x426C54;

	__declspec(naked) void OtherSpawnLimit()
	{
		__asm
		{
			mov eax, [ebx + 0x40]
			cmp eax, maxActiveCount

			jmp dword ptr otherSpawnLimitExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		parser.LoadFile(Globals::configAdvancedPath + "General.ini");

		parser.ParseParameterTable
		(
			"Spawning:Limits",
			Globals::configFormat,
			ConfigParser::FormatParameter<int, Globals::maxHeatLevel>(minActiveCounts, minActiveCount, 0),
			ConfigParser::FormatParameter<int, Globals::maxHeatLevel>(maxActiveCounts, maxActiveCount, 0)
		);

		MemoryEditor::Write<WORD>(0x0D3B,          0x43EB8A); // opcode cmp r,m; cop spawn limit
		MemoryEditor::Write<int*>(&maxActiveCount, 0x43EB8C);

		MemoryEditor::WriteToByteRange(0x90, 0x4442BC, 6); // wave-capacity increment
		MemoryEditor::WriteToByteRange(0x90, 0x42B76B, 6); // cops-lost increment

		MemoryEditor::Write<BYTE>(0x90, 0x57B188,  0x4443E4); // helicopter / roadblock increment
		MemoryEditor::Write<BYTE>(0x00, 0x433CB2);            // min. displayed count
		MemoryEditor::Write<BYTE>(0xEB, 0x42B9AA);            // foreign cops fleeing

		MemoryEditor::DigCodeCave(&ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);
		MemoryEditor::DigCodeCave(&ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);

		MemoryEditor::DigCodeCave(&CopRefill,       copRefillEntrance,       copRefillExit);
		MemoryEditor::DigCodeCave(&FirstCopTable,   firstCopTableEntrance,   firstCopTableExit);
		MemoryEditor::DigCodeCave(&SecondCopTable,  secondCopTableEntrance,  secondCopTableExit);
		MemoryEditor::DigCodeCave(&ThirdCopTable,   thirdCopTableEntrance,   thirdCopTableExit);
		MemoryEditor::DigCodeCave(&FourthCopTable,  fourthCopTableEntrance,  fourthCopTableExit);
		MemoryEditor::DigCodeCave(&FifthCopTable,   fifthCopTableEntrance,   fifthCopTableExit);
		MemoryEditor::DigCodeCave(&OtherSpawnLimit, otherSpawnLimitEntrance, otherSpawnLimitExit);

		featureEnabled = true;
	}



	void SetToHeat(size_t heatLevel)
	{
		if (not featureEnabled) return;

		minActiveCount = minActiveCounts[heatLevel - 1];
		maxActiveCount = maxActiveCounts[heatLevel - 1];
	}
}