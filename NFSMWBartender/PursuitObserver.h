#pragma once

#include <Windows.h>
#include <unordered_map>
#include <unordered_set>
#include <concepts>
#include <utility>
#include <string>
#include <vector>
#include <memory>

#include "Globals.h"
#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "CopSpawnOverrides.h"
#include "CopFleeOverrides.h"
#include "HelicopterOverrides.h"



namespace PursuitObserver
{

	// PursuitObserver class ------------------------------------------------------------------------------------------------------------------------

	class PursuitObserver
	{
		using CopLabel           = PursuitFeatures::CopLabel;
		using CopVehicleReaction = PursuitFeatures::CopVehicleReaction;



	private:

		const address pursuit;

		bool inPursuitUpdatePending    = true;
		bool perHeatLevelUpdatePending = true;

		std::unordered_map<address, CopLabel, Globals::IdentityHash> copVehicleToLabel;
		std::vector<std::unique_ptr<CopVehicleReaction>>             copVehicleReactions;

		inline static std::unordered_set<address, Globals::IdentityHash> copVehiclesLoaded;


		template <std::derived_from<CopVehicleReaction> F, typename ...T>
		void AddFeature(T&& ...args)
		{
			this->copVehicleReactions.push_back(std::make_unique<F>(std::forward<T>(args)...));
		}


		static const char* GetCopName(const address copVehicle)
		{
			return (const char*)(*((address*)(*((address*)(copVehicle + 0x2C)) + 0x24)));
		}


		static CopLabel LabelAddVehicleCall(const address callReturn)
		{
			switch (callReturn)
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
				Globals::Log("WARNING: [OBS] Unknown AddVehicle return address:", callReturn);

			return CopLabel::UNKNOWN;
		}



	public:

		PursuitObserver(const address pursuit) 
			: pursuit(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log("     NEW [OBS] Pursuit", this->pursuit);

			if (CopSpawnOverrides::featureEnabled)
				this->AddFeature<CopSpawnOverrides::ContingentManager>(this->pursuit);

			if (CopFleeOverrides::featureEnabled)    
				this->AddFeature<CopFleeOverrides::MembershipManager>(this->pursuit);

			if (HelicopterOverrides::featureEnabled) 
				this->AddFeature<HelicopterOverrides::HelicopterManager>(this->pursuit);
		}


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log("     DEL [OBS] Pursuit", this->pursuit);
		}


		void UpdateOnHeatChange()
		{
			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->UpdateOnHeatChange();

			this->perHeatLevelUpdatePending = true;
		}


		void UpdateOnGameplay()
		{
			for (const auto& reaction : this->copVehicleReactions)
			{
				if (this->inPursuitUpdatePending)
					reaction.get()->UpdateOnceInPursuit();

				if (this->perHeatLevelUpdatePending)
					reaction.get()->UpdateOncePerHeatLevel();

				reaction.get()->UpdateOnGameplay();
			}

			this->inPursuitUpdatePending    = false;
			this->perHeatLevelUpdatePending = false;
		}


		static size_t GetNumCopsLoaded()
		{
			return PursuitObserver::copVehiclesLoaded.size();
		}


		static void __fastcall Register(const address copVehicle) 
		{
			if constexpr (Globals::loggingEnabled)
			{
				if ((PursuitObserver::copVehiclesLoaded.insert(copVehicle)).second)
				{
					Globals::LogIndent("[REG] +", copVehicle, PursuitObserver::GetCopName(copVehicle));
					Globals::LogIndent("[REG] Cops loaded:", (int)(PursuitObserver::GetNumCopsLoaded()));
				}
			}
			else PursuitObserver::copVehiclesLoaded.insert(copVehicle);
		}


		static void __fastcall Unregister(const address copVehicle)
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (PursuitObserver::copVehiclesLoaded.erase(copVehicle) > 0)
				{
					Globals::LogIndent("[REG] -", copVehicle, PursuitObserver::GetCopName(copVehicle));
					Globals::LogIndent("[REG] Cops loaded:", (int)(PursuitObserver::GetNumCopsLoaded()));
				}
			}
			else PursuitObserver::copVehiclesLoaded.erase(copVehicle);
		}


		static void ClearRegistrations()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogIndent("[REG] Clearing all cops loaded");

			PursuitObserver::copVehiclesLoaded.clear();
		}


		void NotifyFeaturesOfAddition
		(
			const address copVehicle,
			const address addVehicleReturn
		) {
			const CopLabel copLabel     = this->LabelAddVehicleCall(addVehicleReturn);
			const auto     addedVehicle = this->copVehicleToLabel.insert({copVehicle, copLabel});

			if (not addedVehicle.second)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[OBS] =", copVehicle, (int)copLabel, 
						this->GetCopName(copVehicle), "is already", (int)(addedVehicle.first->second));

				return;
			}
			else if (copLabel != CopLabel::HELICOPTER) this->Register(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[OBS] +", copVehicle, (int)copLabel, this->GetCopName(copVehicle));

			const hash copType = Globals::GetCopType(copVehicle);

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->ProcessAddition(copVehicle, copType, copLabel);
		}


		void NotifyFeaturesOfRemoval(const address copVehicle)
		{
			const auto foundVehicle = this->copVehicleToLabel.find(copVehicle);

			if (foundVehicle == this->copVehicleToLabel.end())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log("WARNING: [OBS] Unknown vehicle", copVehicle, this->GetCopName(copVehicle), "in", this->pursuit);

				return;
			}

			const hash copType = Globals::GetCopType(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[OBS] -", copVehicle, (int)(foundVehicle->second), this->GetCopName(copVehicle));

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->ProcessRemoval(copVehicle, copType, foundVehicle->second);

			this->copVehicleToLabel.erase(foundVehicle);
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves
	std::unordered_map<address, PursuitObserver, Globals::IdentityHash> pursuitToObserver;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall CreateObserver(const address pursuit)
	{
		pursuitToObserver.try_emplace(pursuit, pursuit);
	}



	void __fastcall DestroyObserver(const address pursuit)
	{
		pursuitToObserver.erase(pursuit);
	}



	void __fastcall NotifyObserverOfAddition
	(
		const address pursuit,
		const address copVehicle,
		const address addVehicleReturn
	) {
		const auto foundPursuit = pursuitToObserver.find(pursuit);

		if (foundPursuit != pursuitToObserver.end())
			foundPursuit->second.NotifyFeaturesOfAddition(copVehicle, addVehicleReturn);

		else if constexpr (Globals::loggingEnabled)
			Globals::Log("WARNING: [OBS] Addition to unknown pursuit", pursuit);
	}



	void __fastcall NotifyObserverOfRemoval
	(
		const address pursuit,
		const address copVehicle
	) {
		const auto foundPursuit = pursuitToObserver.find(pursuit);

		if (foundPursuit != pursuitToObserver.end())
			foundPursuit->second.NotifyFeaturesOfRemoval(copVehicle);

		else if constexpr (Globals::loggingEnabled)
			Globals::Log("WARNING: [OBS] Removal from unknown pursuit", pursuit);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address eventSpawnEntrance = 0x42E8A8;
	constexpr address eventSpawnExit     = 0x42E8AF;

	__declspec(naked) void EventSpawn()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			mov ecx, esi
			call CopSpawnOverrides::NotifyEventManager // ecx: PVehicle

			mov ecx, esi
			call PursuitObserver::Register // ecx: PVehicle

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
			call PursuitObserver::Register // ecx: PVehicle

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
			call CreateObserver // ecx: AIPursuit
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
			call DestroyObserver // ecx: AIPursuit
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
			call PursuitObserver::Unregister // ecx: copVehicle
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
			call NotifyObserverOfAddition // ecx: AIPursuit; edx: PVehicle; stack: addVehicleReturn
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
			call NotifyObserverOfRemoval // ecx: AIPursuit; edx: PVehicle
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
			push eax
			call PursuitObserver::GetNumCopsLoaded
			mov ecx, eax
			pop eax

			cmp ecx, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr mainSpawnLimitExit
		}
	}



	constexpr address otherSpawnLimitEntrance = 0x426C4E;
	constexpr address otherSpawnLimitExit     = 0x426C54;

	__declspec(naked) void OtherSpawnLimit()
	{
		__asm
		{
			call PursuitObserver::GetNumCopsLoaded
			cmp eax, dword ptr CopSpawnOverrides::maxActiveCounts.current

			jmp dword ptr otherSpawnLimitExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if (not CopSpawnTables::Initialise(parser)) return false;

		CopSpawnOverrides::Initialise  (parser);
		CopFleeOverrides::Initialise   (parser);
		HelicopterOverrides::Initialise(parser);

		MemoryEditor::DigCodeCave(EventSpawn,  eventSpawnEntrance,  eventSpawnExit);
		MemoryEditor::DigCodeCave(PatrolSpawn, patrolSpawnEntrance, patrolSpawnExit);

		MemoryEditor::DigCodeCave(PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);
		MemoryEditor::DigCodeCave(PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
		MemoryEditor::DigCodeCave(VehicleDespawned,   vehicleDespawnedEntrance,   vehicleDespawnedExit);
		MemoryEditor::DigCodeCave(CopRemoved,         copRemovedEntrance,         copRemovedExit);
		MemoryEditor::DigCodeCave(CopAdded,           copAddedEntrance,           copAddedExit);

		MemoryEditor::DigCodeCave(MainSpawnLimit,  mainSpawnLimitEntrance,  mainSpawnLimitExit);
		MemoryEditor::DigCodeCave(OtherSpawnLimit, otherSpawnLimitEntrance, otherSpawnLimitExit);

		return (featureEnabled = true);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		CopSpawnTables::SetToHeat     (isRacing, heatLevel);
		PursuitFeatures::SetToHeat    (isRacing, heatLevel);
		CopSpawnOverrides::SetToHeat  (isRacing, heatLevel);
		CopFleeOverrides::SetToHeat   (isRacing, heatLevel);
		HelicopterOverrides::SetToHeat(isRacing, heatLevel);

		for (auto& pair : pursuitToObserver) 
			pair.second.UpdateOnHeatChange();
	}



	void UpdateState() 
	{
		if (not featureEnabled) return;

		for (auto& pair : pursuitToObserver) 
			pair.second.UpdateOnGameplay();
	}



	void SoftResetState()
	{
		if (not featureEnabled) return;

		PursuitFeatures::ResetState();
		CopSpawnOverrides::ResetState();
	}



	void HardResetState()
	{
		if (not featureEnabled) return;

		SoftResetState();
		PursuitObserver::ClearRegistrations();
	}
}