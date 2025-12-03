#pragma once

#include <concepts>
#include <utility>
#include <vector>
#include <memory>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "CopSpawnOverrides.h"
#include "CopFleeOverrides.h"
#include "HelicopterOverrides.h"
#include "StrategyOverrides.h"
#include "LeaderOverrides.h"



namespace PursuitObserver
{

	// PursuitObserver class ------------------------------------------------------------------------------------------------------------------------

	class PursuitObserver
	{
		using PursuitReaction = PursuitFeatures::PursuitReaction;
		using PursuitCache    = PursuitFeatures::PursuitCache;
		using CopLabel        = PursuitReaction::CopLabel;



	private:

		const address pursuit;

		bool perPursuitUpdatePending   = true;
		bool perHeatLevelUpdatePending = true;

		PursuitCache cache = PursuitCache(this->pursuit);

		HashContainers::AddressMap<CopLabel>          copVehicleToLabel;
		std::vector<std::unique_ptr<PursuitReaction>> pursuitReactions;
		
		// Pointers avoid the moving of Observers on reallocation
		inline static HashContainers::AddressSet                                   copVehiclesLoaded;
		inline static HashContainers::AddressMap<std::unique_ptr<PursuitObserver>> pursuitToObserver;

		inline static constexpr size_t cacheIndex = 0;


		static CopLabel LabelAddVehicleCall(const address caller)
		{
			switch (caller)
			{
			case 0x40B02A: // roadblock cop after spike-strip hit
				[[fallthrough]];

			case 0x4443D8: // regular roadblock cop
				return CopLabel::ROADBLOCK;

			case 0x41F7E6: // LeaderStrategy spawn
				return CopLabel::LEADER;

			case 0x41F426: // HeavyStrategy 3 spawn
				return CopLabel::HEAVY;

			case 0x426BC6: // helicopter
				return CopLabel::HELICOPTER;

			case 0x43EAF5: // free patrol
				[[fallthrough]];

			case 0x43EE97: // first patrol in race
				[[fallthrough]];

			case 0x42E872: // scripted event spawn
				[[fallthrough]];

			case 0x42EB73: // first cop of milestone / bounty pursuit
				[[fallthrough]];

			case 0x4311EC: // pursuit spawn
				return CopLabel::CHASER;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Unknown AddVehicle caller:", caller);

			return CopLabel::UNKNOWN;
		}


		template <std::derived_from<PursuitReaction> R>
		void AddPursuitFeature()
		{
			this->pursuitReactions.push_back(std::make_unique<R>(this->pursuit));
		}


		void UpdateOnHeatChange()
		{
			for (const auto& reaction : this->pursuitReactions)
				reaction->UpdateOnHeatChange();

			this->perHeatLevelUpdatePending = true;
		}


		void UpdateOnGameplay()
		{
			for (const auto& reaction : this->pursuitReactions)
			{
				if (this->perPursuitUpdatePending)
					reaction->UpdateOncePerPursuit();

				if (this->perHeatLevelUpdatePending)
					reaction->UpdateOncePerHeatLevel();

				reaction->UpdateOnGameplay();
			}

			this->perPursuitUpdatePending   = false;
			this->perHeatLevelUpdatePending = false;
		}



	public:

		explicit PursuitObserver(const address pursuit) : pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "PursuitObserver");

			PursuitCache::SetPointer<this->cacheIndex>(this->pursuit, this);

			this->pursuitReactions.reserve(5);

			if (CopSpawnOverrides::featureEnabled)
				this->AddPursuitFeature<CopSpawnOverrides::ChasersManager>();

			if (CopFleeOverrides::featureEnabled)
				this->AddPursuitFeature<CopFleeOverrides::MembershipManager>();

			if (HelicopterOverrides::featureEnabled)
				this->AddPursuitFeature<HelicopterOverrides::HelicopterManager>();

			if (StrategyOverrides::featureEnabled)
				this->AddPursuitFeature<StrategyOverrides::StrategyManager>();

			if (LeaderOverrides::featureEnabled)
				this->AddPursuitFeature<LeaderOverrides::LeaderManager>();
		}


		static void __fastcall AddPursuit(const address pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     NEW [OBS] Pursuit", pursuit);

			const auto [pair, wasAdded] = PursuitObserver::pursuitToObserver.try_emplace(pursuit, nullptr);

			if (wasAdded)
				pair->second = std::make_unique<PursuitObserver>(pursuit);
			
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Duplicate pursuit", pursuit);
		}


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "PursuitObserver");

			PursuitCache::ClearPointer<this->cacheIndex>(this->pursuit, this);
		}


		static void __fastcall RemovePursuit(const address pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     DEL [OBS] Pursuit", pursuit);

			const auto wasRemoved = PursuitObserver::pursuitToObserver.erase(pursuit);

			if constexpr (Globals::loggingEnabled)
			{
				if (not wasRemoved)
					Globals::logger.Log("WARNING: [OBS] Unknown pursuit", pursuit);
			}
		}


		static void NotifyOfHeatChange()
		{
			for (const auto& pair : PursuitObserver::pursuitToObserver)
				pair.second->UpdateOnHeatChange();
		}


		static void NotifyOfGameplay()
		{
			for (const auto& pair : PursuitObserver::pursuitToObserver)
				pair.second->UpdateOnGameplay();
		}


		static void __stdcall ProcessAddedVehicle
		(
			const address pursuit,
			const address copVehicle,
			const address caller
		) {
			const CopLabel copLabel = PursuitObserver::LabelAddVehicleCall(caller);

			if (copLabel != CopLabel::HELICOPTER)
				PursuitObserver::RegisterVehicle(copVehicle);

			const auto observer = PursuitCache::GetPointer<PursuitObserver::cacheIndex, PursuitObserver>(pursuit);

			if (not observer) return; // should never happen

			const auto [pair, wasAdded] = observer->copVehicleToLabel.try_emplace(copVehicle, copLabel);

			if (wasAdded)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[OBS] +", copVehicle, copLabel, Globals::GetVehicleName(copVehicle));

				for (const auto& reaction : observer->pursuitReactions)
					reaction->ReactToAddedVehicle(copVehicle, copLabel);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[OBS] =", copVehicle, copLabel, "is already", pair->second);		
		}


		static void __fastcall ProcessRemovedVehicle
		(
			const address pursuit,
			const address copVehicle
		) {
			const auto observer = PursuitCache::GetPointer<PursuitObserver::cacheIndex, PursuitObserver>(pursuit);

			if (not observer) return; // should never happen

			const auto foundVehicle = observer->copVehicleToLabel.find(copVehicle);

			if (foundVehicle != observer->copVehicleToLabel.end())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[OBS] -", copVehicle, foundVehicle->second, Globals::GetVehicleName(copVehicle));

				for (const auto& reaction : observer->pursuitReactions)
					reaction->ReactToRemovedVehicle(copVehicle, foundVehicle->second);

				observer->copVehicleToLabel.erase(foundVehicle);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Unknown vehicle", copVehicle, Globals::GetVehicleName(copVehicle), "in", pursuit);
		}


		static size_t GetNumRegisteredVehicles()
		{
			return PursuitObserver::copVehiclesLoaded.size();
		}


		static void __fastcall RegisterVehicle(const address copVehicle)
		{
			const auto [pair, wasAdded] = PursuitObserver::copVehiclesLoaded.insert(copVehicle);

			if constexpr (Globals::loggingEnabled)
			{
				if (wasAdded)
				{
					Globals::logger.LogIndent("[REG] +", copVehicle, Globals::GetVehicleName(copVehicle));
					Globals::logger.LogIndent("[REG] Vehicles loaded:", static_cast<int>(PursuitObserver::GetNumRegisteredVehicles()));
				}
			}
		}


		static void __fastcall UnregisterVehicle(const address copVehicle)
		{
			const bool wasRemoved = PursuitObserver::copVehiclesLoaded.erase(copVehicle);

			if constexpr (Globals::loggingEnabled)
			{
				if (wasRemoved)
				{
					Globals::logger.LogIndent("[REG] -", copVehicle, Globals::GetVehicleName(copVehicle));
					Globals::logger.LogIndent("[REG] Vehicles loaded:", static_cast<int>(PursuitObserver::GetNumRegisteredVehicles()));
				}
			}
		}


		static void ClearVehicleRegistrations()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogIndent("[REG] Clearing all registrations");

			PursuitObserver::copVehiclesLoaded.clear();
		}
	};



	

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool> spawnsAreIndependents(false);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	size_t GetNumActiveVehicles()
	{
		return (spawnsAreIndependents.current) ? 0 : PursuitObserver::GetNumRegisteredVehicles();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address pursuitConstructorEntrance = 0x4432D0;
	constexpr address pursuitConstructorExit     = 0x4432D7;

	__declspec(naked) void PursuitConstructor()
	{
		__asm
		{
			add eax, 0x2C
			push eax

			lea ecx, dword ptr [eax + 0x1C]
			call PursuitObserver::AddPursuit // ecx: pursuit

			pop eax

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x8]

			jmp dword ptr pursuitConstructorExit
		}
	}



	constexpr address pursuitDestructorEntrance = 0x433775;
	constexpr address pursuitDestructorExit     = 0x43377A;

	__declspec(naked) void PursuitDestructor()
	{
		__asm
		{
			push ecx

			add ecx, 0x48
			call PursuitObserver::RemovePursuit // ecx: pursuit

			pop ecx

			// Execute original code and resume
			sub esp, 0x8
			push ebx
			push esi

			jmp dword ptr pursuitDestructorExit
		}
	}



	constexpr address vehicleDespawnedEntrance = 0x6693C0;
	constexpr address vehicleDespawnedExit     = 0x6693C7;

	__declspec(naked) void VehicleDespawned()
	{
		__asm
		{
			push ecx

			call PursuitObserver::UnregisterVehicle // ecx: copVehicle

			pop ecx

			// Execute original code and resume
			mov eax, dword ptr [ecx]
			push 0x0
			call dword ptr [eax + 0x34]

			jmp dword ptr vehicleDespawnedExit
		}
	}



	constexpr address copSpawnLimitEntrance = 0x43EB84;
	constexpr address copSpawnLimitExit     = 0x43EB90;

	__declspec(naked) void CopSpawnLimit()
	{
		__asm
		{
			call GetNumActiveVehicles
			cmp eax, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr copSpawnLimitExit
		}
	}



	constexpr address densityFlagEntrance = 0x426C4E;
	constexpr address densityFlagExit     = 0x426C54;

	__declspec(naked) void DensityFlag()
	{
		__asm
		{
			call GetNumActiveVehicles
			cmp eax, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr densityFlagExit
		}
	}



	constexpr address patrolSpawnEntrance = 0x430E37;
	constexpr address patrolSpawnExit     = 0x430E3D;

	__declspec(naked) void PatrolSpawn()
	{
		__asm
		{
			mov ecx, edi
			call PursuitObserver::RegisterVehicle // ecx: copVehicle

			// Execute original code and resume
			inc dword ptr [ebp + 0x94]

			jmp dword ptr patrolSpawnExit
		}
	}



	constexpr address eventSpawnEntrance = 0x42E8A8;
	constexpr address eventSpawnExit     = 0x42E8AF;

	__declspec(naked) void EventSpawn()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			push esi // copVehicle
			mov ecx, offset CopSpawnOverrides::eventSpawns
			call CopSpawnOverrides::Contingent::AddVehicle

			mov ecx, esi
			call PursuitObserver::RegisterVehicle // ecx: copVehicle

			mov al, 0x1

			conclusion:
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x314]

			jmp dword ptr eventSpawnExit
		}
	}



	constexpr address copAddedEntrance = 0x4338A0;
	constexpr address copAddedExit     = 0x4338A5;

	__declspec(naked) void CopAdded()
	{
		__asm
		{
			push ecx

			push dword ptr [esp + 0x4] // caller
			push dword ptr [esp + 0xC] // copVehicle
			push ecx                   // pursuit
			call PursuitObserver::ProcessAddedVehicle

			pop ecx

			// Execute original code and resume
			add ecx, -0x1C
			mov eax, dword ptr [ecx]

			jmp dword ptr copAddedExit
		}
	}



	constexpr address copRemovedEntrance = 0x4338B0;
	constexpr address copRemovedExit     = 0x4338B5;

	__declspec(naked) void CopRemoved()
	{
		__asm
		{
			push ecx

			mov edx, dword ptr [esp + 0x8]
			call PursuitObserver::ProcessRemovedVehicle // ecx: pursuit; edx: copVehicle

			pop ecx

			// Execute original code and resume
			add ecx, -0x1C
			mov eax, dword ptr [ecx]

			jmp dword ptr copRemovedExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not CopSpawnTables::Initialise(parser)) return false;

		parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini");

		// Heat parameters
		HeatParameters::Parse<bool>(parser, "Chasers:Independence", {spawnsAreIndependents});

		// Subheaders
		CopSpawnOverrides  ::Initialise(parser);
		CopFleeOverrides   ::Initialise(parser);
		HelicopterOverrides::Initialise(parser);
		StrategyOverrides  ::Initialise(parser);
		LeaderOverrides    ::Initialise(parser);
		
		// Code modifications 
		MemoryTools::Write<byte>(0x9E, {0x44319C}); // helicopter respawn-timer initialisation

		MemoryTools::MakeRangeNOP(0x443E77, 0x443E87); // helicopter respawn-timer update
		MemoryTools::MakeRangeNOP(0x42B6B4, 0x42B6DF); // reset

		MemoryTools::MakeRangeJMP(PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);
		MemoryTools::MakeRangeJMP(PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
		MemoryTools::MakeRangeJMP(VehicleDespawned,   vehicleDespawnedEntrance,   vehicleDespawnedExit);
		MemoryTools::MakeRangeJMP(CopSpawnLimit,      copSpawnLimitEntrance,      copSpawnLimitExit);
		MemoryTools::MakeRangeJMP(DensityFlag,        densityFlagEntrance,        densityFlagExit);
		MemoryTools::MakeRangeJMP(PatrolSpawn,        patrolSpawnEntrance,        patrolSpawnExit);
		MemoryTools::MakeRangeJMP(EventSpawn,         eventSpawnEntrance,         eventSpawnExit);
		MemoryTools::MakeRangeJMP(CopRemoved,         copRemovedEntrance,         copRemovedExit);
		MemoryTools::MakeRangeJMP(CopAdded,           copAddedEntrance,           copAddedExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		CopSpawnTables::Validate();

		if constexpr (Globals::loggingEnabled)
			CopSpawnOverrides::LogConfigurationReport();

		HelicopterOverrides::Validate();
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		spawnsAreIndependents.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [OBS] PursuitObserver");
			Globals::logger.LogLongIndent("spawnsAreIndependent    ", spawnsAreIndependents.current);
		}

		CopSpawnTables     ::SetToHeat(isRacing, heatLevel);
		CopSpawnOverrides  ::SetToHeat(isRacing, heatLevel);
		CopFleeOverrides   ::SetToHeat(isRacing, heatLevel);
		HelicopterOverrides::SetToHeat(isRacing, heatLevel);
		StrategyOverrides  ::SetToHeat(isRacing, heatLevel);
		LeaderOverrides    ::SetToHeat(isRacing, heatLevel);

		PursuitObserver::NotifyOfHeatChange();
	}



	void UpdateState() 
	{
		if (not featureEnabled) return;

		PursuitObserver::NotifyOfGameplay();
	}



	void SoftResetState()
	{
		if (not featureEnabled) return;

		CopSpawnOverrides::ResetState();
	}



	void HardResetState()
	{
		if (not featureEnabled) return;

		CopSpawnOverrides::ResetState();

		PursuitObserver::ClearVehicleRegistrations();
	}
}