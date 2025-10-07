#pragma once

#include <concepts>
#include <utility>
#include <vector>
#include <memory>

#include "Globals.h"
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
		using CopLabel        = PursuitReaction::CopLabel;
		using Cache           = PursuitFeatures::Cache;



	private:

		const address pursuit;

		bool perPursuitUpdatePending   = true;
		bool perHeatLevelUpdatePending = true;

		Cache pursuitData = Cache(this->pursuit);

		HashContainers::AddressMap<CopLabel>          copVehicleToLabel;
		std::vector<std::unique_ptr<PursuitReaction>> pursuitReactions;

		inline static HashContainers::AddressSet                  copVehiclesLoaded;
		inline static HashContainers::AddressMap<PursuitObserver> pursuitToObserver;

		inline static const auto GetVehicleName = (const char* (__thiscall*)(address))0x688090;


		static PursuitObserver* GetInstance(const address pursuit)
		{
			const Cache* const cache = Cache::GetPointer(pursuit);

			if ((not cache) or (not cache->pursuitObserver))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [OBS] Missing instance in", pursuit);

				return nullptr;
			}

			return (PursuitObserver*)(cache->pursuitObserver);
		}


		static CopLabel LabelAddVehicleCall(const address caller)
		{
			switch (caller)
			{
			case 0x40B02A: // roadblock cop after spike-strip hit
				[[fallthrough]];

			case 0x4443D8: // lone roadblock cop
				return CopLabel::ROADBLOCK;

			case 0x41F7E6: // LeaderStrategy spawn
				return CopLabel::LEADER;

			case 0x41F426: // HeavyStrategy 3 spawn
				return CopLabel::HEAVY;

			case 0x426BC6: // helicopter
				return CopLabel::HELICOPTER;

			case 0x43EAF5: // non-pursuit patrol
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
				Globals::logger.Log("WARNING: [OBS] Unknown AddVehicle return address:", caller);

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
				reaction.get()->UpdateOnHeatChange();

			this->perHeatLevelUpdatePending = true;
		}


		void UpdateOnGameplay()
		{
			for (const auto& reaction : this->pursuitReactions)
			{
				if (this->perPursuitUpdatePending)
					reaction.get()->UpdateOncePerPursuit();

				if (this->perHeatLevelUpdatePending)
					reaction.get()->UpdateOncePerHeatLevel();

				reaction.get()->UpdateOnGameplay();
			}

			this->perPursuitUpdatePending   = false;
			this->perHeatLevelUpdatePending = false;
		}



	public:

		explicit PursuitObserver(const address pursuit) : pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     NEW [OBS] Pursuit", this->pursuit);

			this->pursuitData.pursuitObserver = (address)this;

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
			const auto addedPursuit = PursuitObserver::pursuitToObserver.try_emplace(pursuit, pursuit);
			
			if constexpr (Globals::loggingEnabled)
			{
				if (not addedPursuit.second)
					Globals::logger.Log("WARNING: [OBS] Duplicate pursuit", pursuit);
			}
		}


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     DEL [OBS] Pursuit", this->pursuit);
		}


		static void __fastcall RemovePursuit(const address pursuit)
		{
			const bool wasPursuit = PursuitObserver::pursuitToObserver.erase(pursuit);

			if constexpr (Globals::loggingEnabled)
			{
				if (not wasPursuit)
					Globals::logger.Log("WARNING: [OBS] Unknown pursuit", pursuit);
			}
		}


		static void NotifyOfHeatChange()
		{
			for (auto& pair : PursuitObserver::pursuitToObserver)
				pair.second.UpdateOnHeatChange();
		}


		static void NotifyOfGameplay()
		{
			for (auto& pair : PursuitObserver::pursuitToObserver)
				pair.second.UpdateOnGameplay();
		}


		static void __stdcall ProcessAddedVehicle
		(
			const address pursuit,
			const address copVehicle,
			const address caller
		) {
			PursuitObserver* const observer = PursuitObserver::GetInstance(pursuit);

			if (not observer) return;

			const CopLabel copLabel     = PursuitObserver::LabelAddVehicleCall(caller);
			const auto     addedVehicle = observer->copVehicleToLabel.insert({copVehicle, copLabel});

			if (not addedVehicle.second)
			{
				if constexpr (Globals::loggingEnabled)
				{
					Globals::logger.Log
					(
						pursuit,
						"[OBS] =",
						copVehicle,
						(int)copLabel,
						PursuitObserver::GetVehicleName(copVehicle),
						"is already",
						(int)(addedVehicle.first->second)
					);
				}

				return;
			}

			if (copLabel != CopLabel::HELICOPTER)
				observer->RegisterVehicle(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[OBS] +", copVehicle, (int)copLabel, PursuitObserver::GetVehicleName(copVehicle));

			for (const auto& reaction : observer->pursuitReactions)
				reaction->ReactToAddedVehicle(copVehicle, copLabel);
		}


		static void __fastcall ProcessRemovedVehicle
		(
			const address pursuit,
			const address copVehicle
		) {
			PursuitObserver* const observer = PursuitObserver::GetInstance(pursuit);

			if (not observer) return;

			const auto foundVehicle = observer->copVehicleToLabel.find(copVehicle);

			if (foundVehicle == observer->copVehicleToLabel.end())
			{
				if constexpr (Globals::loggingEnabled)
				{
					Globals::logger.Log
					(
						"WARNING: [OBS] Unknown vehicle",
						copVehicle,
						PursuitObserver::GetVehicleName(copVehicle),
						"in",
						pursuit
					);
				}

				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[OBS] -", copVehicle, (int)(foundVehicle->second), PursuitObserver::GetVehicleName(copVehicle));

			for (const auto& reaction : observer->pursuitReactions)
				reaction->ReactToRemovedVehicle(copVehicle, foundVehicle->second);

			observer->copVehicleToLabel.erase(foundVehicle);
		}


		static size_t GetNumRegisteredVehicles()
		{
			return PursuitObserver::copVehiclesLoaded.size();
		}


		static void __fastcall RegisterVehicle(const address copVehicle)
		{
			const bool isNew = (PursuitObserver::copVehiclesLoaded.insert(copVehicle)).second;

			if constexpr (Globals::loggingEnabled)
			{
				if (isNew)
				{
					Globals::logger.LogIndent("[REG] +", copVehicle, PursuitObserver::GetVehicleName(copVehicle));
					Globals::logger.LogIndent("[REG] Vehicles loaded:", (int)(PursuitObserver::GetNumRegisteredVehicles()));
				}
			}
		}


		static void __fastcall UnregisterVehicle(const address copVehicle)
		{
			const bool wasRegistered = (PursuitObserver::copVehiclesLoaded.erase(copVehicle) > 0);

			if constexpr (Globals::loggingEnabled)
			{
				if (wasRegistered)
				{
					Globals::logger.LogIndent("[REG] -", copVehicle, PursuitObserver::GetVehicleName(copVehicle));
					Globals::logger.LogIndent("[REG] Vehicles loaded:", (int)(PursuitObserver::GetNumRegisteredVehicles()));
				}
			}
		}


		static void ClearVehicleRegistrations()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogIndent("[REG] Clearing all vehicles loaded");

			PursuitObserver::copVehiclesLoaded.clear();
		}
	};



	

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool> spawnsAreIndependents(false);





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address eventSpawnEntrance = 0x42E8A8;
	constexpr address eventSpawnExit     = 0x42E8AF;

	__declspec(naked) void EventSpawn()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			push esi // copVehicle
			mov ecx, offset CopSpawnOverrides::events
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



	constexpr address mainSpawnLimitEntrance = 0x43EB84;
	constexpr address mainSpawnLimitExit     = 0x43EB90;

	__declspec(naked) void MainSpawnLimit()
	{
		__asm
		{
			cmp byte ptr spawnsAreIndependents.current, 0x1
			je independent // chasers are independent

			call PursuitObserver::GetNumRegisteredVehicles
			jmp conclusion // chasers weren't independent

			independent:
			xor eax, eax

			conclusion:
			cmp eax, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr mainSpawnLimitExit
		}
	}



	constexpr address otherSpawnLimitEntrance = 0x426C4E;
	constexpr address otherSpawnLimitExit     = 0x426C54;

	__declspec(naked) void OtherSpawnLimit()
	{
		__asm
		{
			cmp byte ptr spawnsAreIndependents.current, 0x1
			je independent // chasers are independent

			call PursuitObserver::GetNumRegisteredVehicles
			jmp conclusion // chasers weren't independent

			independent:
			xor eax, eax

			conclusion:
			cmp eax, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr otherSpawnLimitExit
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
		
		// Code caves
		MemoryEditor::Write<byte>(0x9E, {0x44319C});                 // helicopter respawn-timer initialisation
		MemoryEditor::WriteToAddressRange(0x90, 0x443E77, 0x443E87); // update
		MemoryEditor::WriteToAddressRange(0x90, 0x42B6B4, 0x42B6DF); // reset

		MemoryEditor::DigCodeCave(EventSpawn,  eventSpawnEntrance,  eventSpawnExit);
		MemoryEditor::DigCodeCave(PatrolSpawn, patrolSpawnEntrance, patrolSpawnExit);

		MemoryEditor::DigCodeCave(PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);
		MemoryEditor::DigCodeCave(PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
		MemoryEditor::DigCodeCave(VehicleDespawned,   vehicleDespawnedEntrance,   vehicleDespawnedExit);
		MemoryEditor::DigCodeCave(CopRemoved,         copRemovedEntrance,         copRemovedExit);
		MemoryEditor::DigCodeCave(CopAdded,           copAddedEntrance,           copAddedExit);

		MemoryEditor::DigCodeCave(MainSpawnLimit,  mainSpawnLimitEntrance,  mainSpawnLimitExit);
		MemoryEditor::DigCodeCave(OtherSpawnLimit, otherSpawnLimitEntrance, otherSpawnLimitExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		CopSpawnTables     ::Validate();
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

		PursuitFeatures::heatLevelKnown = true;

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

		PursuitFeatures::heatLevelKnown = false;
		CopSpawnOverrides::ResetState();
	}



	void HardResetState()
	{
		if (not featureEnabled) return;

		SoftResetState();
		PursuitObserver::ClearVehicleRegistrations();
	}
}