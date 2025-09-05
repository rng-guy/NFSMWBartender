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
		


	private:

		const address pursuit;

		bool inPursuitUpdatePending    = true;
		bool perHeatLevelUpdatePending = true;

		HashContainers::AddressMap<CopLabel>          copVehicleToLabel;
		std::vector<std::unique_ptr<PursuitReaction>> pursuitReactions;

		inline static HashContainers::AddressSet                  copVehiclesLoaded;
		inline static HashContainers::AddressMap<PursuitObserver> pursuitToObserver;

		inline static const char* (__thiscall* const GetVehicleName)(address) = (const char* (__thiscall*)(address))0x688090;


		template <std::derived_from<PursuitReaction> R>
		void AddPursuitFeature()
		{
			this->pursuitReactions.push_back(std::make_unique<R>(this->pursuit));
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
				if (this->inPursuitUpdatePending)
					reaction.get()->UpdateOnceInPursuit();

				if (this->perHeatLevelUpdatePending)
					reaction.get()->UpdateOncePerHeatLevel();

				reaction.get()->UpdateOnGameplay();
			}

			this->inPursuitUpdatePending = false;
			this->perHeatLevelUpdatePending = false;
		}


		void ProcessAddedVehicle
		(
			const address copVehicle,
			const address returnAddress
		) {
			const CopLabel copLabel     = this->LabelAddVehicleCall(returnAddress);
			const auto     addedVehicle = this->copVehicleToLabel.insert({ copVehicle, copLabel });

			if (not addedVehicle.second)
			{
				if constexpr (Globals::loggingEnabled)
				{
					Globals::logger.Log
					(
						this->pursuit, 
						"[OBS] =", 
						copVehicle, 
						(int)copLabel,
						this->GetVehicleName(copVehicle), 
						"is already", 
						(int)(addedVehicle.first->second)
					);
				}

				return;
			}

			if (copLabel != CopLabel::HELICOPTER)
				this->RegisterVehicle(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[OBS] +", copVehicle, (int)copLabel, this->GetVehicleName(copVehicle));

			for (const auto& reaction : this->pursuitReactions)
				reaction->ReactToAddedVehicle(copVehicle, copLabel);
		}


		void ProcessRemovedVehicle(const address copVehicle)
		{
			const auto foundVehicle = this->copVehicleToLabel.find(copVehicle);

			if (foundVehicle == this->copVehicleToLabel.end())
			{
				if constexpr (Globals::loggingEnabled)
				{
					Globals::logger.Log
					(
						"WARNING: [OBS] Unknown vehicle", 
						copVehicle, 
						this->GetVehicleName(copVehicle), 
						"in", 
						this->pursuit
					);
				}

				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[OBS] -", copVehicle, (int)(foundVehicle->second), this->GetVehicleName(copVehicle));

			for (const auto& reaction : this->pursuitReactions)
				reaction->ReactToRemovedVehicle(copVehicle, foundVehicle->second);

			this->copVehicleToLabel.erase(foundVehicle);
		}



	public:

		explicit PursuitObserver(const address pursuit) : pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     NEW [OBS] Pursuit", this->pursuit);
		}


		static void __fastcall AddPursuit(const address pursuit)
		{
			const auto addedPursuit = PursuitObserver::pursuitToObserver.try_emplace(pursuit, pursuit);
			
			if (addedPursuit.second)
			{
				PursuitObserver& newObserver = addedPursuit.first->second;

				if (CopSpawnOverrides::featureEnabled)
					newObserver.AddPursuitFeature<CopSpawnOverrides::ChasersManager>();

				if (CopFleeOverrides::featureEnabled)
					newObserver.AddPursuitFeature<CopFleeOverrides::MembershipManager>();

				if (HelicopterOverrides::featureEnabled)
					newObserver.AddPursuitFeature<HelicopterOverrides::HelicopterManager>();

				if (StrategyOverrides::featureEnabled)
					newObserver.AddPursuitFeature<StrategyOverrides::StrategyManager>();

				if (LeaderOverrides::featureEnabled)
					newObserver.AddPursuitFeature<LeaderOverrides::LeaderManager>();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Duplicate pursuit addition", pursuit);
		}


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     DEL [OBS] Pursuit", this->pursuit);
		}


		static void __fastcall RemovePursuit(const address pursuit)
		{
			PursuitObserver::pursuitToObserver.erase(pursuit);
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


		static void __fastcall NotifyOfAddedVehicle
		(
			const address pursuit,
			const address copVehicle,
			const address caller
		) {
			const auto foundPursuit = PursuitObserver::pursuitToObserver.find(pursuit);

			if (foundPursuit != PursuitObserver::pursuitToObserver.end())
				foundPursuit->second.ProcessAddedVehicle(copVehicle, caller);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Addition to unknown pursuit", pursuit);
		}


		static void __fastcall NotifyOfRemovedVehicle
		(
			const address pursuit,
			const address copVehicle
		) {
			const auto foundPursuit = PursuitObserver::pursuitToObserver.find(pursuit);

			if (foundPursuit != PursuitObserver::pursuitToObserver.end())
				foundPursuit->second.ProcessRemovedVehicle(copVehicle);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Removal from unknown pursuit", pursuit);
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



	

	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address eventSpawnEntrance = 0x42E8A8;
	constexpr address eventSpawnExit     = 0x42E8AF;

	__declspec(naked) void EventSpawn()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			push esi
			lea ecx, dword ptr [CopSpawnOverrides::events]
			call CopSpawnOverrides::Contingent::AddVehicle // stack: copVehicle

			mov ecx, esi
			call PursuitObserver::RegisterVehicle // ecx: PVehicle

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
			// Execute original code first
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
			mov edx, dword ptr [esp + 0x8]
			push dword ptr [esp + 0x4]
			call PursuitObserver::NotifyOfAddedVehicle // ecx: pursuit; edx: copVehicle; stack: caller
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
			call PursuitObserver::NotifyOfRemovedVehicle // ecx: pursuit; edx: copVehicle
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
			call PursuitObserver::GetNumRegisteredVehicles
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
			call PursuitObserver::GetNumRegisteredVehicles
			cmp eax, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr otherSpawnLimitExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not CopSpawnTables::Initialise(parser)) return false;

		// Subheaders
		CopSpawnOverrides  ::Initialise(parser);
		CopFleeOverrides   ::Initialise(parser);
		HelicopterOverrides::Initialise(parser);
		StrategyOverrides  ::Initialise(parser);
		LeaderOverrides    ::Initialise(parser);
		
		// Code caves
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

		CopSpawnTables     ::SetToHeat(isRacing, heatLevel);
		PursuitFeatures    ::SetToHeat(isRacing, heatLevel);
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

		PursuitFeatures  ::ResetState();
		CopSpawnOverrides::ResetState();
	}



	void HardResetState()
	{
		if (not featureEnabled) return;

		SoftResetState();
		PursuitObserver::ClearVehicleRegistrations();
	}
}