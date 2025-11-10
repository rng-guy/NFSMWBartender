#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"
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
			const HeatParameters::Pair<CopSpawnTables::SpawnTable>& tables,
			const address                                           pursuit = 0x0
		) 
			: source(tables.current), pursuit(pursuit)
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
			const vault copType   = PursuitFeatures::GetVehicleType(copVehicle);
			const auto  addedType = this->copTypeToCurrentCount.insert({copType, 1});

			if (not addedType.second)
				++(addedType.first->second);

			this->UpdateSpawnTableCapacity(copType, -1);
			++(this->numActiveCops);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->pursuit)
					Globals::logger.Log(this->pursuit, "[CON] Type ratio:", addedType.first->second, '/', this->numActiveCops);
			}
		}


		bool RemoveVehicle(const address copVehicle)
		{
			const vault copType   = PursuitFeatures::GetVehicleType(copVehicle);
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


		bool HasAvailableCop() const
		{
			return this->table.HasCapacity();
		}


		const char* GetNameOfAvailableCop() const
		{
			return this->table.GetRandomCopName();
		}


		const char* GetNameOfSpawnableCop() const
		{
			const char* const copName = this->GetNameOfAvailableCop();
			return (copName) ? copName : this->source->GetRandomCopName();
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<int> minActiveCounts(1);
	HeatParameters::Pair<int> maxActiveCounts(8);

	// Code caves
	bool skipEventSpawns = true;

	Contingent eventSpawns    (CopSpawnTables::eventSpawnTables);
	Contingent roadblockSpawns(CopSpawnTables::roadblockSpawnTables);





	// ChaserManager class --------------------------------------------------------------------------------------------------------------------------

	class ChasersManager : public PursuitFeatures::PursuitReaction
	{
		using PursuitCache = PursuitFeatures::PursuitCache;



	private:

		int maxNumPatrolCars = 0;
		
		int& fullWaveCapacity  = *((int*)(this->pursuit + 0x144));
		int& numCopsLostInWave = *((int*)(this->pursuit + 0x14C));

		const int&   pursuitStatus  = *((int*)(this->pursuit + 0x218));
		const bool&  isPerpBusted   = *((bool*)(this->pursuit + 0xE8));
		const bool&  bailingPursuit = *((bool*)(this->pursuit + 0xE9));
		const float& spawnCooldown  = *((float*)(this->pursuit + 0xCC));

		Contingent chaserSpawns = Contingent(CopSpawnTables::chaserSpawnTables, this->pursuit);

		inline static constexpr size_t cacheIndex = 1;


		void UpdateSpawnTable()
		{
			if (PursuitFeatures::heatLevelKnown)
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
			const address attribute = PursuitFeatures::GetFromPursuitlevel(this->pursuit, 0x24F7A1BC); // fetches "NumPatrolCars"

			this->maxNumPatrolCars = (attribute) ? *((int*)attribute) : 0;

			if constexpr (Globals::loggingEnabled)
			{
				if (attribute)
					Globals::logger.Log(this->pursuit, "[SPA] Max. patrol cars:", this->maxNumPatrolCars);

				else
					Globals::logger.Log("WARNING: [SPA] Invalid numPatrolCars pointer in", this->pursuit);
			}
		}


		bool IsSearchModeActive() const 
		{
			return (this->pursuitStatus == 2);
		}


		int GetWaveCapacity() const
		{
			return this->fullWaveCapacity - (this->numCopsLostInWave + this->chaserSpawns.GetNumActiveCops());
		}


		void CorrectWaveCapacity() const
		{
			if (this->fullWaveCapacity == 0) return;

			const int waveCapacity = this->GetWaveCapacity();

			if (waveCapacity < 0)
			{
				this->fullWaveCapacity -= waveCapacity;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SPA] Wave correction:", -waveCapacity);
			}
		}


		bool CanNewChaserSpawn() const
		{
			if (not PursuitFeatures::heatLevelKnown) return false;

			if (this->isPerpBusted)   return false;
			if (this->bailingPursuit) return false;

			if (this->spawnCooldown > 0.f)                                        return false;
			if (not this->chaserSpawns.HasAvailableCop())                         return false;
			if (this->chaserSpawns.GetNumActiveCops() >= maxActiveCounts.current) return false;

			if (this->IsSearchModeActive())
				return (this->chaserSpawns.GetNumActiveCops() < this->maxNumPatrolCars);

			return ((this->GetWaveCapacity() > 0) or (this->chaserSpawns.GetNumActiveCops() < minActiveCounts.current));
		}



	public:

		explicit ChasersManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "ChasersManager");

			PursuitCache::SetPointer<this->cacheIndex>(this->pursuit, this);
		}


		~ChasersManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "ChasersManager");

			PursuitCache::ClearPointer<this->cacheIndex>(this->pursuit, this);
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
				this->chaserSpawns.AddVehicle(copVehicle);
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
					++(this->numCopsLostInWave);
			}
		}


		static bool __fastcall GetChaserSpawnStatus(const address pursuit)
		{
			const auto manager = PursuitCache::GetPointer<ChasersManager::cacheIndex, ChasersManager>(pursuit);
			return (manager) ? manager->CanNewChaserSpawn() : false;
		}


		static const char* __fastcall GetNameOfNewChaser(const address pursuit)
		{
			const auto manager = PursuitCache::GetPointer<ChasersManager::cacheIndex, ChasersManager>(pursuit);
			return (manager and manager->CanNewChaserSpawn()) ? manager->chaserSpawns.GetNameOfAvailableCop() : nullptr;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	const char* __fastcall GetByClassReplacement(const address caller)
	{
		if (PursuitFeatures::heatLevelKnown)
		{
			switch (caller)
			{
			case 0x4269E6: // helicopter
				return HelicopterOverrides::HelicopterManager::GetHelicopterVehicle();

			case 0x42EAAD: // first cop of milestone / bounty pursuit
				[[fallthrough]];

			case 0x430DAD: // free-roam patrol
				return CopSpawnTables::patrolSpawnTables.current->GetRandomCopName();
				
			case 0x43E049: // roadblock
				return roadblockSpawns.GetNameOfSpawnableCop();
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SPA] Unknown ByClass return address:", caller);
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
			call ChasersManager::GetNameOfNewChaser // ecx: pursuit

			jmp dword ptr copRequestExit
		}
	}



	constexpr address requestCheckEntrance = 0x426C56;
	constexpr address requestCheckExit     = 0x426C5F;

	__declspec(naked) void RequestCheck()
	{
		__asm
		{
			mov ecx, edi
			call ChasersManager::GetChaserSpawnStatus // ecx: pursuit
			test al, al

			jmp dword ptr requestCheckExit
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



	constexpr address eventSpawnResetEntrance = 0x58E7E0;
	constexpr address eventSpawnResetExit     = 0x58E7E6;

	__declspec(naked) void EventSpawnReset()
	{
		__asm
		{
			add esp, 0x4
			push eax

			mov ecx, offset eventSpawns
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
			cmp dword ptr [esp + 0x70], 0x42E72E
			jne retain // not "Events" spawn

			cmp byte ptr skipEventSpawns, 0x0
			jne retain // skip "Events" spawn

			cmp byte ptr PursuitFeatures::heatLevelKnown, 0x1
			jne retain // Heat level unknown

			mov ecx, offset eventSpawns
			call Contingent::GetNameOfSpawnableCop
			mov ebp, eax
			jmp conclusion

			retain:
			mov ebp, dword ptr [esp + 0x74]

			conclusion:
			test ebp, ebp

			jmp dword ptr byNameInterceptorExit
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
		parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini");

		// Heat parameters
		HeatParameters::Parse<int, int>(parser, "Chasers:Limits", {minActiveCounts, 0}, {maxActiveCounts, 1});

		// Code caves
		MemoryTools::WriteToByteRange(0x90, 0x4442BC, 6); // wave-capacity increment
		MemoryTools::WriteToByteRange(0x90, 0x42B76B, 6); // cops-lost increment

		MemoryTools::WriteToAddressRange(0x90, 0x4440D7, 0x4440DF); // membership check

		MemoryTools::Write<byte>(0x00, {0x433CB2});           // min. displayed count
		MemoryTools::Write<byte>(0x90, {0x57B188, 0x4443E4}); // helicopter / roadblock increment

		MemoryTools::DigCodeCave(CopRefill,          copRefillEntrance,          copRefillExit);
		MemoryTools::DigCodeCave(CopRequest,         copRequestEntrance,         copRequestExit);
		MemoryTools::DigCodeCave(RequestCheck,       requestCheckEntrance,       requestCheckExit);
		MemoryTools::DigCodeCave(RoadblockSpawn,     roadblockSpawnEntrance,     roadblockSpawnExit);
		MemoryTools::DigCodeCave(EventSpawnReset,    eventSpawnResetEntrance,    eventSpawnResetExit);
		MemoryTools::DigCodeCave(ByNameInterceptor,  byNameInterceptorEntrance,  byNameInterceptorExit);
		MemoryTools::DigCodeCave(ByClassInterceptor, byClassInterceptorEntrance, byClassInterceptorExit);
		
		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log          ("    HEAT [SPA] CopSpawnOverrides");
		Globals::logger.LogLongIndent("activeCount             ", minActiveCounts.current, "to", maxActiveCounts.current);
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

		eventSpawns    .UpdateSpawnTable();
		roadblockSpawns.UpdateSpawnTable();

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		skipEventSpawns = true;

		eventSpawns    .ClearVehicles();
		roadblockSpawns.ClearVehicles();
	}
}