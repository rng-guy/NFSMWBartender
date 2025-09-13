#pragma once

#include "Globals.h"
#include "HashContainers.h"
#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "HelicopterOverrides.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace CopSpawnOverrides
{

	// Contingent class -----------------------------------------------------------------------------------------------------------------------------

	class Contingent
	{
		using TablePair = HeatParameters::Pair<CopSpawnTables::SpawnTable>;



	private:

		const address          pursuit;
		const TablePair* const source;

		int numActiveCops = 0;

		CopSpawnTables::SpawnTable table = *(this->source->current);

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
			const TablePair& source,
			const address    pursuit = 0x0
		) 
			: source(&source), pursuit(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log(this->pursuit, "[CON] New contingent");
			}
		}


		void UpdateSpawnTable()
		{
			this->table = *(this->source->current);

			for (const auto& pair : this->copTypeToCurrentCount)
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (this->pursuit)
					{
						const char* const copName = this->table.TypeToName(pair.first);

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
			const vault copType   = Globals::GetVehicleType(copVehicle);
			const auto  addedType = this->copTypeToCurrentCount.insert({copType, 1});

			if (not addedType.second)
				(addedType.first->second)++;

			this->UpdateSpawnTableCapacity(copType, -1);
			(this->numActiveCops)++;

			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log(this->pursuit, "[CON] Type ratio:", addedType.first->second, '/', this->numActiveCops);
			}
		}


		bool RemoveVehicle(const address copVehicle)
		{
			const vault copType   = Globals::GetVehicleType(copVehicle);
			const auto  foundType = this->copTypeToCurrentCount.find(copType);

			if (foundType != this->copTypeToCurrentCount.end())
			{
				(foundType->second)--;
				
				this->UpdateSpawnTableCapacity(copType, +1);
				(this->numActiveCops)--;

				if constexpr (Globals::loggingEnabled)
				{
					if (this->pursuit)
						Globals::logger.Log(this->pursuit, "[CON] Type ratio:", foundType->second, '/', this->numActiveCops);
				}

				if (foundType->second < 1)
					this->copTypeToCurrentCount.erase(foundType);

				return true;
			}
			else if constexpr (Globals::loggingEnabled)
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


		const char* GetNameOfAvailableCop() const
		{
			return this->table.GetRandomCopName();
		}


		const char* GetNameOfSpawnableCop() const
		{
			const char* const copName = this->GetNameOfAvailableCop();
			return (copName) ? copName : this->source->current->GetRandomCopName();
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<int> minActiveCounts(1);
	HeatParameters::Pair<int> maxActiveCounts(8);

	// Code caves
	const char* const placeholderVehicle = "copmidsize";

	bool skipEventSpawns = true;

	Contingent events    (CopSpawnTables::eventSpawnTables);
	Contingent roadblocks(CopSpawnTables::roadblockSpawnTables);





	// ChaserManager class --------------------------------------------------------------------------------------------------------------------------

	class ChasersManager : public PursuitFeatures::PursuitReaction
	{
	private:

		int numPatrolCarsToSpawn = 0;
		
		int* const       fullWaveCapacity  = (int*)(this->pursuit + 0x144);
		int* const       numCopsLostInWave = (int*)(this->pursuit + 0x14C);
		const int* const pursuitStatus     = (int*)(this->pursuit + 0x218);

		Contingent chasers = Contingent(CopSpawnTables::chaserSpawnTables, this->pursuit);

		inline static HashContainers::AddressMap<ChasersManager*> pursuitToManager;


		void UpdateSpawnTable()
		{
			if (this->IsHeatLevelKnown())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SPA] Updating table");

				this->chasers.UpdateSpawnTable();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SPA] Skipping updating spawn table");
		}


		void UpdateNumPatrolCars()
		{
			this->numPatrolCarsToSpawn = 0;

			// PursuitAttributes node
			static const auto GetPursuitNode = (address (__thiscall*)(address))0x418E90;
			const address     pursuitNode    = GetPursuitNode(this->pursuit);
			
			if (not pursuitNode)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [SPA] Invalid pursuitNode pointer in", this->pursuit);

				return;
			}

			// Number of patrol cars
			static const auto GetPursuitAttribute = (address (__thiscall*)(address, vault, int))0x454810;
			const address     attribute           = GetPursuitAttribute(pursuitNode, 0x24F7A1BC, 0);      // fetches "NumPatrolCars"

			if (attribute)
			{
				this->numPatrolCarsToSpawn = *((int*)attribute);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SPA] Max. patrol cars:", this->numPatrolCarsToSpawn);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Invalid numPatrolCars pointer in", this->pursuit);
		}


		bool IsSearchModeActive() const 
		{
			return (*(this->pursuitStatus) == 2);
		}


		int GetWaveCapacity() const
		{
			return *(this->fullWaveCapacity) - (*(this->numCopsLostInWave) + this->chasers.GetNumActiveCops());
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
			if (this->IsSearchModeActive())
				return (not (this->chasers.GetNumActiveCops() < this->numPatrolCarsToSpawn));

			else
				return (not ((this->GetWaveCapacity() > 0) or (this->chasers.GetNumActiveCops() < minActiveCounts.current)));
		}



	public:

		explicit ChasersManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			this->pursuitToManager.insert({this->pursuit, this});
		}


		~ChasersManager() override
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


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			skipEventSpawns = false;

			if (copLabel == CopLabel::CHASER)
				this->chasers.AddVehicle(copVehicle);
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
				if (this->chasers.RemoveVehicle(copVehicle))
					(*(this->numCopsLostInWave))++;
			}
		}


		static const char* GetNameOfAvailableChaser(const address pursuit)
		{
			if (not ChasersManager::IsHeatLevelKnown()) return nullptr;

			const auto foundPursuit = ChasersManager::pursuitToManager.find(pursuit);

			if (foundPursuit != ChasersManager::pursuitToManager.end())
			{
				if (not foundPursuit->second->IsWaveExhausted())
					return foundPursuit->second->chasers.GetNameOfAvailableCop();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Chaser request for unknown pursuit", pursuit);
			
			return nullptr;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	const char* __fastcall GetByClassReplacement(const address spawnReturn)
	{
		if (ChasersManager::IsHeatLevelKnown())
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
				return roadblocks.GetNameOfSpawnableCop();
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
			return ((not skipEventSpawns) and ChasersManager::IsHeatLevelKnown());
				
		case 0x43EBD0: // chaser / "COOLDOWN" patrol
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
			return events.GetNameOfSpawnableCop();

		case 0x43EBD0: // chaser / "COOLDOWN" patrol
			return ChasersManager::GetNameOfAvailableChaser(pursuit);
		}

		return nullptr;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

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



	constexpr address copRequestEntrance = 0x42BA50;
	constexpr address copRequestExit     = 0x42BCED;

	__declspec(naked) void CopRequest()
	{
		__asm
		{
			mov al, byte ptr [ecx + 0xE8]
			test al, al
			jne deny // perp busted

			fldz
			fcomp dword ptr [ecx + 0xCC]
			fnstsw ax
			test ah, 0x1
			jne deny // spawn cooldown > 0

			mov al, byte ptr [ecx + 0xE9]
			test al, al
			jne deny

			mov eax, dword ptr placeholderVehicle
			jmp conclusion // was valid request

			deny:
			xor eax, eax

			conclusion:
			jmp dword ptr copRequestExit
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
			push eax
			lea ecx, dword ptr [roadblocks]
			call Contingent::AddVehicle // stack: copVehicle
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

			lea ecx, dword ptr [roadblocks]
			call Contingent::ClearVehicles

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
			lea ecx, dword ptr [events]
			call Contingent::ClearVehicles
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





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini");

		// Heat parameters
		HeatParameters::Parse<int, int>(parser, "Chasers:Limits", {minActiveCounts, 0}, {maxActiveCounts, 1});

		// Code caves
		MemoryEditor::WriteToByteRange(0x90, 0x4442BC, 6); // wave-capacity increment
		MemoryEditor::WriteToByteRange(0x90, 0x42B76B, 6); // cops-lost increment

		MemoryEditor::WriteToAddressRange(0x90, 0x4440D7, 0x4440DF); // membership check

		MemoryEditor::Write<byte>(0x00, {0x433CB2});           // min. displayed count
		MemoryEditor::Write<byte>(0x90, {0x57B188, 0x4443E4}); // helicopter / roadblock increment

		MemoryEditor::DigCodeCave(CopRefill,          copRefillEntrance,          copRefillExit);
		MemoryEditor::DigCodeCave(CopRequest,         copRequestEntrance,         copRequestExit);
		MemoryEditor::DigCodeCave(RoadblockSpawn,     roadblockSpawnEntrance,     roadblockSpawnExit);
		MemoryEditor::DigCodeCave(EventSpawnReset,    eventSpawnResetEntrance,    eventSpawnResetExit);
		MemoryEditor::DigCodeCave(ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);
		MemoryEditor::DigCodeCave(ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);
		
		// Status flag
		featureEnabled = true;

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		minActiveCounts.SetToHeat(isRacing, heatLevel);
		maxActiveCounts.SetToHeat(isRacing, heatLevel);

		if (isRacing) 
			skipEventSpawns = false;

		events    .UpdateSpawnTable();
		roadblocks.UpdateSpawnTable();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [SPA] CopSpawnOverrides");
			Globals::logger.LogLongIndent("activeCount             ", minActiveCounts.current, "to", maxActiveCounts.current);
		}
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		skipEventSpawns = true;

		events    .ClearVehicles();
		roadblocks.ClearVehicles();
	}
}