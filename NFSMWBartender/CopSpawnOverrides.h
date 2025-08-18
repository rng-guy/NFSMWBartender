#pragma once

#include "Globals.h"
#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "HelicopterOverrides.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace CopSpawnOverrides
{

	// GlobalSpawnManager class ---------------------------------------------------------------------------------------------------------------------

	class GlobalSpawnManager
	{
	private:

		Globals::VaultMap<int> copTypeToCurrentCount;

		CopSpawnTables::SpawnTable               table;
		const CopSpawnTables::SpawnTable** const source;



	public:

		explicit GlobalSpawnManager(const CopSpawnTables::SpawnTable** const source) : source(source) {}


		void ReloadSpawnTable()
		{
			if (*(this->source))
			{
				this->table = *(*(this->source));

				for (const auto& pair : this->copTypeToCurrentCount)
					this->table.UpdateCapacity(pair.first, -(pair.second));
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [GLO] Failed to reload table");
		}


		void ResetSpawnTable()
		{
			for (const auto& pair : this->copTypeToCurrentCount)
				this->table.UpdateCapacity(pair.first, pair.second);

			this->copTypeToCurrentCount.clear();
		}


		const char* GetRandomCopName()
		{
			const char* copName = this->table.GetRandomCopName();

			if ((not copName) and *(this->source))
			{
				copName = (*(*(this->source))).GetRandomCopName();

				if constexpr (Globals::loggingEnabled)
				{
					if (not copName)
						Globals::logger.Log("WARNING: [GLO] Failed to select vehicle");
				}
			}
			else if constexpr (Globals::loggingEnabled)
			{
				if (not copName)
					Globals::logger.Log("WARNING: [GLO] Invalid source-table pointer");
			}

			return copName;
		}


		void NotifyOfSpawn(const address copVehicle)
		{
			const vault copType   = Globals::GetVehicleType(copVehicle);
			const auto  addedType = this->copTypeToCurrentCount.insert({copType, 1});

			if (not addedType.second) 
				(addedType.first->second)++;

			this->table.UpdateCapacity(copType, -1);
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<int> minActiveCounts(1);
	HeatParameters::Pair<int> maxActiveCounts(8);

	// Code caves
	bool skipEventSpawns = true;

	GlobalSpawnManager eventManager    (&(CopSpawnTables::eventSpawnTables.current));
	GlobalSpawnManager roadblockManager(&(CopSpawnTables::roadblockSpawnTables.current));





	// ContingentManager class ----------------------------------------------------------------------------------------------------------------------

	class ContingentManager : public PursuitFeatures::CopVehicleReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:

		const address pursuit;

		int numPatrolCarsToSpawn = 0;
		int numCopsInContingent  = 0;

		int* const fullWaveCapacity  = (int*)(pursuit + 0x144);
		int* const numCopsLostInWave = (int*)(pursuit + 0x14C);

		CopSpawnTables::SpawnTable table;
		Globals::VaultMap<int>     copTypeToCurrentCount;

		inline static Globals::AddressMap<ContingentManager*> pursuitToManager;


		void UpdateSpawnTable()
		{
			if (not this->IsHeatLevelKnown())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SPA] Skipping updating spawn table");

				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SPA] Updating table");

			this->table = *(CopSpawnTables::pursuitSpawnTables.current);

			for (const auto& pair : this->copTypeToCurrentCount)
			{
				if constexpr (Globals::loggingEnabled)
				{
					const char* const copName = this->table.TypeToName(pair.first);

					if (copName)
						Globals::logger.Log(this->pursuit, "[SPA] Copying", pair.second, copName);

					else
						Globals::logger.Log(this->pursuit, "[SPA] Copying", pair.second, pair.first);
				}

				this->table.UpdateCapacity(pair.first, -(pair.second));

				if constexpr (Globals::loggingEnabled)
				{
					if (this->table.Contains(pair.first))
						Globals::logger.Log(this->pursuit, "[SPA] Type capacity:", this->table.GetCapacity(pair.first));

					else
						Globals::logger.Log(this->pursuit, "[SPA] Type capacity undefined");
				}
			}
		}


		void UpdateNumPatrolCars()
		{
			this->numPatrolCarsToSpawn = 0;

			// PursuitAttributes node
			static address (__thiscall* const GetPursuitNode)     (address)             = (address (__thiscall*)(address))            0x418E90;
			static address (__thiscall* const GetPursuitParameter)(address, vault, int) = (address (__thiscall*)(address, vault, int))0x454810;

			const address pursuitNode = GetPursuitNode(this->pursuit);
			
			if (not pursuitNode)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [SPA] Invalid pursuitNode pointer in", this->pursuit);

				return;
			}

			// Number of patrol cars
			const address numPatrolCars = GetPursuitParameter(pursuitNode, 0x24F7A1BC, 0); // fetches "NumPatrolCars"

			if (numPatrolCars)
			{
				this->numPatrolCarsToSpawn = *((int*)numPatrolCars);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SPA] Max. patrol cars:", this->numPatrolCarsToSpawn);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Invalid numPatrolCars pointer in", this->pursuit);
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
					Globals::logger.Log(this->pursuit, "[SPA] Wave correction:", -waveCapacity);
			}
		}


		bool IsWaveExhausted() const
		{
			const int minCount = (this->IsSearchModeActive()) ? this->numPatrolCarsToSpawn : minActiveCounts.current;
			return (not ((this->GetWaveCapacity() > 0) or (this->numCopsInContingent < minCount)));
		}



	public:

		static const ContingentManager* GetInstance(const address pursuit)
		{
			const auto foundPursuit = ContingentManager::pursuitToManager.find(pursuit);
			return (foundPursuit != ContingentManager::pursuitToManager.end()) ? foundPursuit->second : nullptr;
		}


		explicit ContingentManager(const address pursuit)
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
			const CopLabel copLabel
		) 
			override
		{
			skipEventSpawns = false;

			if (copLabel != CopLabel::CHASER) return;

			const vault copType   = Globals::GetVehicleType(copVehicle);
			const auto  addedType = this->copTypeToCurrentCount.insert({copType, 1});
			if (not addedType.second) (addedType.first->second)++;

			this->table.UpdateCapacity(copType, -1);
			(this->numCopsInContingent)++;

			if constexpr (Globals::loggingEnabled)
			{
				if (this->table.Contains(copType))
					Globals::logger.Log(this->pursuit, "[SPA] Type capacity:", this->table.GetCapacity(copType));

				else
					Globals::logger.Log(this->pursuit, "[SPA] Type capacity undefined");

				Globals::logger.Log(this->pursuit, "[SPA] Type ratio:", addedType.first->second, '/', this->numCopsInContingent);
			}
		}


		void ProcessRemoval
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel != CopLabel::CHASER) return;

			const vault copType   = Globals::GetVehicleType(copVehicle);
			const auto  foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType != this->copTypeToCurrentCount.end())
			{
				(foundType->second)--;
				(this->numCopsInContingent)--;
				(*(this->numCopsLostInWave))++;

				this->table.UpdateCapacity(copType, +1);

				if constexpr (Globals::loggingEnabled)
				{
					if (this->table.Contains(copType))
						Globals::logger.Log(this->pursuit, "[SPA] Type capacity:", this->table.GetCapacity(copType));

					else
						Globals::logger.Log(this->pursuit, "[SPA] Type capacity undefined");

					Globals::logger.Log(this->pursuit, "[SPA] Type ratio:", foundType->second, '/', this->numCopsInContingent);
				}

				if (foundType->second < 1)
					this->copTypeToCurrentCount.erase(foundType);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Unknown type", copType, "in", this->pursuit);
		}


		const char* GetRandomCopName() const
		{		
			return (this->IsWaveExhausted()) ? nullptr : this->table.GetRandomCopName();
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
				return HelicopterOverrides::helicopterVehicles.current;

			case 0x42EAAD: // first cop of milestone / bounty pursuit
				[[fallthrough]];

			case 0x430DAD: // free-roam patrol
				return CopSpawnTables::patrolSpawnTables.current->GetRandomCopName();
				
			case 0x43E049: // roadblock
				return roadblockManager.GetRandomCopName();
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Unknown ByClass return address:", spawnReturn);
		}

		return nullptr;
	}



	bool __fastcall IsByNameInterceptable(const address spawnReturn)
	{
		switch (spawnReturn)
		{
		case 0x42E72E: // scripted event spawn
			return ((not skipEventSpawns) and ContingentManager::IsHeatLevelKnown());
				
		case 0x43EBD0: // chaser / "Cooldown" patrol
			return true;
		}

		return false;
	}



	const char* __fastcall GetByNameReplacement
	(
		const address spawnReturn,
		const address pursuit
	) {
		switch (spawnReturn)
		{
		case 0x42E72E: // scripted event spawn
			return eventManager.GetRandomCopName();

		case 0x43EBD0: // chaser / "Cooldown" patrol
			if (not ContingentManager::IsHeatLevelKnown()) break;
			const ContingentManager* const manager = ContingentManager::GetInstance(pursuit);
			if (manager) return manager->GetRandomCopName();
		}

		return nullptr;
	}





	// Code caves --------------------------------------------------------------------------------------------------------------------------------------

	constexpr address copRefillEntrance = 0x4442AE;
	constexpr address copRefillExit     = 0x4442B6;

	__declspec(naked) void CopRefill()
	{
		__asm
		{
			cmp dword ptr minActiveCounts.current, 0x0
			je conclusion // zero is valid engagement count

			inc edx
			mov dword ptr [esi + 0x184], edx
			inc eax

			conclusion:
			jmp dword ptr copRefillExit
		}
	}



	constexpr address rammingGoalEntrance = 0x409850;
	constexpr address rammingGoalExit     = 0x409857;

	__declspec(naked) void RammingGoal()
	{
		__asm
		{
			mov eax, dword ptr [ecx + 0x48]
			cmp eax, 0x73B87374
			je conclusion // current goal is AIGoalHeadOnRam

			// Execute original code and resume
			mov eax, dword ptr [esp + 0x4]
			mov dword ptr [ecx + 0x48], eax

			conclusion:
			jmp dword ptr rammingGoalExit
		}
	}



	constexpr address roadblockSpawnEntrance = 0x43E04F;
	constexpr address roadblockSpawnExit     = 0x43E06C;

	__declspec(naked) void RoadblockSpawn()
	{
		static constexpr address roadblockSpawnSkip    = 0x43E031;
		static constexpr address addVehicleToRoadblock = 0x43C4E0;

		__asm
		{
			je conclusion // spawn intended to fail

			push eax
			mov ecx, eax
			call NotifyRoadblockManager // ecx: copVehicle
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

			mov ecx, dword ptr [esp + 0x70]
			call IsByNameInterceptable // ecx: return address
			test al, al
			je conclusion              // do not intercept

			mov ecx, dword ptr [esp + 0x70]
			mov edx, esi
			call GetByNameReplacement // ecx: return address; edx: AIPursuit
			mov ebp, eax

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
		static constexpr address getPursuitVehicleByName = 0x41ECD0;

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



	constexpr address firstCopTableEntrance = 0x424205;
	constexpr address firstCopTableExit     = 0x424213;

	__declspec(naked) void FirstCopTable()
	{
		__asm
		{
			mov edx, dword ptr CopSpawnTables::currentMaxCopCapacity // COUNT

			jmp dword ptr firstCopTableExit
		}
	}



	constexpr address secondCopTableEntrance = 0x424298;
	constexpr address secondCopTableExit     = 0x4242D5;

	__declspec(naked) void SecondCopTable()
	{
		__asm
		{
			mov ebx, dword ptr CopSpawnTables::currentMaxCopCapacity // COUNT

			jmp dword ptr secondCopTableExit
		}
	}



	constexpr address thirdCopTableEntrance = 0x4242F9;
	constexpr address thirdCopTableExit     = 0x424323;

	__declspec(naked) void ThirdCopTable()
	{
		__asm
		{
			xor edx, edx

			lea ecx, dword ptr [ecx + ecx * 0x2]
			mov dword ptr [ebp + ecx * 0x8 + 0xC], edx // DWORD 4

			lea ecx, dword ptr [ebp + ecx * 0x8 + 0x0]
			mov dword ptr [ecx], edx       // DWORD 1
			mov dword ptr [ecx + 0x4], edx // DWORD 2
			mov dword ptr [ecx + 0x8], edx // DWORD 3

			mov edx, dword ptr CopSpawnTables::currentMaxCopCapacity // COUNT
			mov dword ptr [ecx + 0x10], edx

			mov edx, 0x1 // CHANCE
			mov dword ptr [ecx + 0x14], edx

			jmp dword ptr thirdCopTableExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini");

		// Pursuit parameters
		HeatParameters::Parse<int, int>(parser, "Spawning:Limits", {minActiveCounts, 0}, {maxActiveCounts, 0});

		// Code caves
		MemoryEditor::WriteToByteRange(0x90, 0x4442BC, 6); // wave-capacity increment
		MemoryEditor::WriteToByteRange(0x90, 0x42B76B, 6); // cops-lost increment

		MemoryEditor::Write<byte>(0x00, 0x433CB2);           // min. displayed count
		MemoryEditor::Write<byte>(0xEB, 0x42BB2D);           // helicopter spawns in "COOLDOWN" mode 
		MemoryEditor::Write<byte>(0x90, 0x57B188, 0x4443E4); // helicopter / roadblock increment
		MemoryEditor::Write<byte>(0xEB, 0x42B9AA, 0x44389E); // non-member / HeavyStrategy cops fleeing

		MemoryEditor::DigCodeCave(CopRefill,          copRefillEntrance,          copRefillExit);
		MemoryEditor::DigCodeCave(RammingGoal,        rammingGoalEntrance,        rammingGoalExit);
		MemoryEditor::DigCodeCave(RoadblockSpawn,     roadblockSpawnEntrance,     roadblockSpawnExit);
		MemoryEditor::DigCodeCave(EventSpawnReset,    eventSpawnResetEntrance,    eventSpawnResetExit);
		MemoryEditor::DigCodeCave(ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);
		MemoryEditor::DigCodeCave(ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);

		MemoryEditor::DigCodeCave(FirstCopTable,  firstCopTableEntrance,  firstCopTableExit);
		MemoryEditor::DigCodeCave(SecondCopTable, secondCopTableEntrance, secondCopTableExit);
		MemoryEditor::DigCodeCave(ThirdCopTable,  thirdCopTableEntrance,  thirdCopTableExit);

		return (featureEnabled = true);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		minActiveCounts.SetToHeat(isRacing, heatLevel);
		maxActiveCounts.SetToHeat(isRacing, heatLevel);

		if (isRacing) skipEventSpawns = false;

		eventManager    .ReloadSpawnTable();
		roadblockManager.ReloadSpawnTable();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [SPA] CopSpawnOverrides");

			Globals::logger.LogLongIndent("minActiveCount          ", minActiveCounts.current);
			Globals::logger.LogLongIndent("maxActiveCount          ", maxActiveCounts.current);
		}
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		skipEventSpawns = true;

		eventManager    .ResetSpawnTable();
		roadblockManager.ResetSpawnTable();
	}
}