#pragma once

#include <Windows.h>
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
	private:

		const address pursuit;

		bool inPursuitUpdatePending    = true;
		bool perHeatLevelUpdatePending = true;

		std::unordered_set<address> supportCopVehicles;

		std::vector<std::unique_ptr<PursuitFeatures::CopVehicleReaction>> copVehicleReactions;

		inline static std::unordered_map<address, address> vehicleToPursuit;
		inline static hash (__thiscall* const GetCopType)(address) = (hash (__thiscall*)(address))0x6880A0;


		template <std::derived_from<PursuitFeatures::CopVehicleReaction> F, typename ...T>
		void AddFeature(T&& ...args)
		{
			this->copVehicleReactions.push_back(std::make_unique<F>(std::forward<T>(args)...));
		}


		static const char* GetCopName(const address copVehicle)
		{
			return (const char*)(*(address*)(*(address*)(copVehicle + 0x2C) + 0x24));
		}


		bool IsFromRoadblock(const address copVehicle) const
		{
			return (copVehicle == *(address*)(this->pursuit + 0x78));
		}


		bool IsSupport(const address copVehicle)
		{
			if (this->supportCopVehicles.contains(copVehicle)) return true;

			static bool (__thiscall* const IsGroundSupport)(address, address) = (bool (__thiscall*)(address, address))0x419890;

			if (not (IsGroundSupport(this->pursuit, copVehicle) or this->IsFromRoadblock(copVehicle))) return false;

			this->supportCopVehicles.insert(copVehicle);

			return true;
		}


		static bool IsHelicopter(const address copVehicle)
		{
			static constexpr key chopperClass = 0xB80933AA;
			return (*(key*)(copVehicle + 0x6C) == chopperClass);
		}


		PursuitFeatures::CopLabel Label(const address copVehicle)
		{
			if (this->IsHelicopter(copVehicle))
				return PursuitFeatures::CopLabel::HELICOPTER;

			else if (this->IsSupport(copVehicle))
				return PursuitFeatures::CopLabel::SUPPORT;

			else return PursuitFeatures::CopLabel::CHASER;
		}



	public:

		PursuitObserver(const address pursuit) 
			: pursuit(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: Creating instance");

			if (CopSpawnOverrides::featureEnabled)
				this->AddFeature<CopSpawnOverrides::ContingentManager>(this->pursuit);

			if (CopFleeOverrides::featureEnabled)    
				this->AddFeature<CopFleeOverrides::MembershipManager>(this->pursuit);

			if (HelicopterOverrides::featureEnabled) 
				this->AddFeature<HelicopterOverrides::HelicopterManager>(this->pursuit);
		};


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: Deleting instance");

			std::erase_if(this->vehicleToPursuit, [&](const auto& pair) {return (pair.second == this->pursuit);});

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: Registrations:", (int)(this->GetNumRegistrations()));
		}


		void UpdateOnHeatChange()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: Heat change");

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


		void NotifyOfFreeRoam()
		{
			this->perHeatLevelUpdatePending = true;
		}


		static size_t GetNumRegistrations()
		{
			return PursuitObserver::vehicleToPursuit.size();
		}


		static void __fastcall Register
		(
			const address pursuit,
			const address copVehicle
		) {
			const auto foundVehicle = PursuitObserver::vehicleToPursuit.find(copVehicle);

			if (foundVehicle != PursuitObserver::vehicleToPursuit.end())
			{
				if (not (foundVehicle->second))
				{
					if constexpr (Globals::loggingEnabled)
						Globals::Log(pursuit, "OBS: Re-registering", copVehicle);

					foundVehicle->second = pursuit;
				}
				else if constexpr (Globals::loggingEnabled)
					if (pursuit) Globals::Log("WARNING: Failed re-registration", copVehicle, foundVehicle->second, pursuit);
			}
			else
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log(pursuit, "OBS: Registering", copVehicle);

				PursuitObserver::vehicleToPursuit.insert({copVehicle, pursuit});

				if constexpr (Globals::loggingEnabled)
					Globals::Log(pursuit, "OBS: Registrations:", (int)(PursuitObserver::GetNumRegistrations()));
			}
		}


		static void __fastcall Unregister(const address copVehicle)
		{
			if constexpr (Globals::loggingEnabled)
			{
				const auto foundVehicle = PursuitObserver::vehicleToPursuit.find(copVehicle);
				if (foundVehicle == PursuitObserver::vehicleToPursuit.end()) return;

				const address pursuit = foundVehicle->second;

				Globals::Log(pursuit, "OBS: Unregistering", copVehicle);
				PursuitObserver::vehicleToPursuit.erase(foundVehicle);
				Globals::Log(pursuit, "OBS: Registrations:", (int)(PursuitObserver::GetNumRegistrations()));
			}
			else PursuitObserver::vehicleToPursuit.erase(copVehicle);
		}


		static void ClearRegistrations()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log((address)0x0, "OBS: Clearing registrations");

			PursuitObserver::vehicleToPursuit.clear();

			if constexpr (Globals::loggingEnabled)
				Globals::Log((address)0x0, "OBS: Registrations:", (int)(PursuitObserver::GetNumRegistrations()));
		}


		void NotifyFeaturesOfAddition(const address copVehicle)
		{
			const PursuitFeatures::CopLabel copLabel = this->Label(copVehicle);
			const hash                      copType  = this->GetCopType(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: +", copVehicle, (int)copLabel, this->GetCopName(copVehicle));

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->ProcessAddition(copVehicle, copType, copLabel);

			if (copLabel != PursuitFeatures::CopLabel::HELICOPTER) 
				this->Register(this->pursuit, copVehicle);
		}


		void NotifyFeaturesOfRemoval(const address copVehicle)
		{
			const PursuitFeatures::CopLabel copLabel = this->Label(copVehicle);
			const hash                      copType  = this->GetCopType(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: -", copVehicle, (int)copLabel, this->GetCopName(copVehicle));

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->ProcessRemoval(copVehicle, copType, copLabel);

			if (copLabel == PursuitFeatures::CopLabel::SUPPORT)
				this->supportCopVehicles.erase(copVehicle);
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	std::unordered_map<address, PursuitObserver> pursuitToObserver;





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
		const address copVehicle
	) {
		const auto foundPursuit = pursuitToObserver.find(pursuit);
		if (foundPursuit == pursuitToObserver.end()) return;

		foundPursuit->second.NotifyFeaturesOfAddition(copVehicle);
	}



	void __fastcall NotifyObserverOfRemoval
	(
		const address pursuit,
		const address copVehicle
	) {
		const auto foundPursuit = pursuitToObserver.find(pursuit);
		if (foundPursuit == pursuitToObserver.end()) return;

		foundPursuit->second.NotifyFeaturesOfRemoval(copVehicle);
	}



	void NotifyObserversOfFreeRoam()
	{
		for (auto& observer : pursuitToObserver)
			observer.second.NotifyOfFreeRoam();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address eventSpawnEntrance = 0x42E8A8;
	const address eventSpawnExit     = 0x42E8AF;

	__declspec(naked) void EventSpawn()
	{
		__asm
		{
			test al, al
			je conclusion // spawn failed

			push eax

			mov ecx, esi
			call CopSpawnOverrides::NotifyEventManager // ecx: PVehicle

			xor ecx, ecx
			mov edx, esi
			call PursuitObserver::Register // ecx: AIPursuit; edx: PVehicle

			pop eax

			conclusion:
			// Execute original code and resume
			mov ecx, [esp + 0x314]

			jmp dword ptr eventSpawnExit
		}
	}



	const address patrolSpawnEntrance = 0x430E37;
	const address patrolSpawnExit     = 0x430E3D;

	__declspec(naked) void PatrolSpawn()
	{
		__asm
		{
			xor ecx, ecx
			mov edx, edi
			call PursuitObserver::Register // ecx: AIPursuit; edx: PVehicle

			// Execute original code and resume
			inc dword ptr [ebp + 0x94]

			jmp dword ptr patrolSpawnExit
		}
	}



	const address pursuitConstructorEntrance = 0x4432D0;
	const address pursuitConstructorExit     = 0x4432D7;

	__declspec(naked) void PursuitConstructor()
	{
		__asm
		{
			// Execute original code first
			add eax, 0x2C
			
			push eax
			lea ecx, [eax + 0x1C]
			call CreateObserver // ecx: AIPursuit
			pop eax

			// Execute original code and resume
			mov ecx, [esp + 0x8]

			jmp dword ptr pursuitConstructorExit
		}
	}



	const address pursuitDestructorEntrance = 0x433775;
	const address pursuitDestructorExit     = 0x43377A;

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



	const address vehicleDespawnedEntrance = 0x6693C0;
	const address vehicleDespawnedExit     = 0x6693C7;

	__declspec(naked) void VehicleDespawned()
	{
		__asm
		{
			push ecx
			call PursuitObserver::Unregister // ecx: copVehicle
			pop ecx

			// Execute original code and resume
			mov eax, [ecx]
			push 0x0
			call dword ptr [eax + 0x34]

			jmp dword ptr vehicleDespawnedExit
		}
	}



	const address enterFreeRoamEntrance = 0x5FE3E0;
	const address enterFreeRoamExit     = 0x5FE3E7;

	__declspec(naked) void EnterFreeRoam()
	{
		__asm
		{
			push ecx
			call NotifyObserversOfFreeRoam
			pop ecx

			// Execute original code and resume
			push -0x1
			push 0x8760BE

			jmp dword ptr enterFreeRoamExit
		}
	}



	const address copAddedEntrance = 0x4338A0;
	const address copAddedExit     = 0x4338A5;

	__declspec(naked) void CopAdded()
	{
		__asm
		{
			push ecx
			mov edx, [esp + 0x8]
			call NotifyObserverOfAddition // ecx: AIPursuit; edx: PVehicle
			pop ecx

			// Execute original code and resume
			add ecx, -0x1C
			mov eax, [ecx]

			jmp dword ptr copAddedExit
		}
	}



	const address copRemovedEntrance = 0x4338B0;
	const address copRemovedExit     = 0x4338B5;

	__declspec(naked) void CopRemoved()
	{
		__asm
		{
			push ecx
			mov edx, [esp + 0x8]
			call NotifyObserverOfRemoval // ecx: AIPursuit; edx: PVehicle
			pop ecx

			// Execute original code and resume
			add ecx, -0x1C
			mov eax, [ecx]

			jmp dword ptr copRemovedExit
		}
	}



	const address mainSpawnLimitEntrance = 0x43EB84;
	const address mainSpawnLimitExit     = 0x43EB90;

	__declspec(naked) void MainSpawnLimit()
	{
		__asm
		{
			push eax
			call PursuitObserver::GetNumRegistrations
			mov ecx, eax
			pop eax

			cmp ecx, CopSpawnOverrides::maxActiveCount

			jmp dword ptr mainSpawnLimitExit
		}
	}



	const address otherSpawnLimitEntrance = 0x426C4E;
	const address otherSpawnLimitExit     = 0x426C54;

	__declspec(naked) void OtherSpawnLimit()
	{
		__asm
		{
			call PursuitObserver::GetNumRegistrations
			cmp eax, CopSpawnOverrides::maxActiveCount

			jmp dword ptr otherSpawnLimitExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		CopSpawnTables::Initialise(parser);
		if (not CopSpawnTables::featureEnabled) return;

		CopSpawnOverrides::Initialise(parser);
		CopFleeOverrides::Initialise(parser);
		HelicopterOverrides::Initialise(parser);

		MemoryEditor::DigCodeCave(&EventSpawn,  eventSpawnEntrance,  eventSpawnExit);
		MemoryEditor::DigCodeCave(&PatrolSpawn, patrolSpawnEntrance, patrolSpawnExit);

		MemoryEditor::DigCodeCave(&PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);
		MemoryEditor::DigCodeCave(&PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
		MemoryEditor::DigCodeCave(&VehicleDespawned,   vehicleDespawnedEntrance,   vehicleDespawnedExit);
		MemoryEditor::DigCodeCave(&EnterFreeRoam,      enterFreeRoamEntrance,      enterFreeRoamExit);
		MemoryEditor::DigCodeCave(&CopRemoved,         copRemovedEntrance,         copRemovedExit);
		MemoryEditor::DigCodeCave(&CopAdded,           copAddedEntrance,           copAddedExit);

		MemoryEditor::DigCodeCave(&MainSpawnLimit,  mainSpawnLimitEntrance,  mainSpawnLimitExit);
		MemoryEditor::DigCodeCave(&OtherSpawnLimit, otherSpawnLimitEntrance, otherSpawnLimitExit);

		featureEnabled = true;
	}



	void SetToHeat(size_t heatLevel) 
	{
		if (not featureEnabled) return;

		CopSpawnTables::SetToHeat(heatLevel);

		PursuitFeatures::SetToHeat(heatLevel);
		CopSpawnOverrides::SetToHeat(heatLevel);
		CopFleeOverrides::SetToHeat(heatLevel);
		HelicopterOverrides::SetToHeat(heatLevel);

		for (auto& pair : pursuitToObserver) 
			pair.second.UpdateOnHeatChange();
	}



	void UpdateState() 
	{
		if (not featureEnabled) return;

		for (auto& pair : pursuitToObserver) 
			pair.second.UpdateOnGameplay();
	}



	void ResetState()
	{
		if (not featureEnabled) return;

		PursuitFeatures::ResetState();
		CopSpawnOverrides::ResetState();
		PursuitObserver::ClearRegistrations();
	}
}