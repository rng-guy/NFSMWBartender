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

		std::unordered_set<address> chaserCopVehicles;
		std::unordered_set<address> supportCopVehicles;

		std::vector<std::unique_ptr<PursuitFeatures::CopVehicleReaction>> copVehicleReactions;

		inline static hash (__thiscall* const GetCopType)(address) = (hash (__thiscall*)(address))0x6880A0;


		template <std::derived_from<PursuitFeatures::CopVehicleReaction> F, typename ...T>
		void AddFeature(T&& ...args)
		{
			this->copVehicleReactions.push_back(std::make_unique<F>(std::forward<T>(args)...));
		}


		static const char* GetName(const address copVehicle)
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
			if (this->chaserCopVehicles.contains(copVehicle))
				return PursuitFeatures::CopLabel::CHASER;

			else if (this->IsHelicopter(copVehicle))
				return PursuitFeatures::CopLabel::HELICOPTER;

			else if (this->IsSupport(copVehicle))
				return PursuitFeatures::CopLabel::SUPPORT;

			this->chaserCopVehicles.insert(copVehicle);
			return PursuitFeatures::CopLabel::CHASER;
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


		size_t GetNumActiveCopVehicles() const 
		{
			return (this->chaserCopVehicles.size() + this->supportCopVehicles.size());
		}


		void NotifyFeaturesOfAddition(const address copVehicle)
		{
			const PursuitFeatures::CopLabel copLabel = this->Label(copVehicle);
			const hash                      copType  = this->GetCopType(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: +", copVehicle, (int)copLabel, this->GetName(copVehicle));

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->ProcessAddition(copVehicle, copType, copLabel);

			if constexpr (Globals::loggingEnabled)
				if (copLabel != PursuitFeatures::CopLabel::HELICOPTER)
					Globals::Log(this->pursuit, "OBS: Active cop vehicles:", (int)(this->GetNumActiveCopVehicles()));
		}


		void NotifyFeaturesOfRemoval(const address copVehicle)
		{
			const PursuitFeatures::CopLabel copLabel = this->Label(copVehicle);
			const hash                      copType  = this->GetCopType(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: -", copVehicle, (int)copLabel, this->GetName(copVehicle));

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->ProcessRemoval(copVehicle, copType, copLabel);

			this->chaserCopVehicles.erase(copVehicle);
			this->supportCopVehicles.erase(copVehicle);

			if constexpr (Globals::loggingEnabled)
				if (copLabel != PursuitFeatures::CopLabel::HELICOPTER)
					Globals::Log(this->pursuit, "OBS: Active cop vehicles:", (int)(this->GetNumActiveCopVehicles()));
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	std::unordered_map<address, std::unique_ptr<PursuitObserver>> pursuitToObserver;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall CreateObserver(const address pursuit)
	{
		pursuitToObserver.insert({pursuit, std::make_unique<PursuitObserver>(pursuit)});
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

		foundPursuit->second.get()->NotifyFeaturesOfAddition(copVehicle);
	}



	void __fastcall NotifyObserverOfRemoval
	(
		const address pursuit,
		const address copVehicle
	) {
		const auto foundPursuit = pursuitToObserver.find(pursuit);
		if (foundPursuit == pursuitToObserver.end()) return;

		foundPursuit->second.get()->NotifyFeaturesOfRemoval(copVehicle);
	}



	size_t GetTotalNumActiveCopVehicles()
	{
		size_t result = 0;

		for (const auto& pair : pursuitToObserver)
			result += pair.second.get()->GetNumActiveCopVehicles();

		return result;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address pursuitConstructorEntrance = 0x4432D0;
	const address pursuitConstructorExit     = 0x4432D7;

	__declspec(naked) void PursuitConstructor()
	{
		__asm
		{
			// Execute original code first
			add eax, 0x2C
			mov ecx, [esp + 0x8]

			push eax
			push ecx
			push edx

			lea ecx, [eax + 0x1C]
			call CreateObserver // ecx: AIPursuit

			pop edx
			pop ecx
			pop eax

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
			push edx

			add ecx, 0x48
			call DestroyObserver // ecx: AIPursuit
			
			pop edx
			pop ecx

			// Execute original code and resume
			sub esp, 0x8
			push ebx
			push esi

			jmp dword ptr pursuitDestructorExit
		}
	}



	const address copAddedEntrance = 0x4338A0;
	const address copAddedExit     = 0x4338A5;

	__declspec(naked) void CopAdded()
	{
		__asm
		{
			push ecx
			push edx

			mov edx, [esp + 0xC]
			call NotifyObserverOfAddition // ecx: AIPursuit; edx: PVehicle
			
			pop edx
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
			push edx

			mov edx, [esp + 0xC]
			call NotifyObserverOfRemoval // ecx: AIPursuit; edx: PVehicle

			pop edx
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

			call GetTotalNumActiveCopVehicles
			mov ecx, eax
			cmp ecx, CopSpawnOverrides::maxActiveCount

			pop eax

			jmp dword ptr mainSpawnLimitExit
		}
	}



	const address otherSpawnLimitEntrance = 0x426C4E;
	const address otherSpawnLimitExit     = 0x426C54;

	__declspec(naked) void OtherSpawnLimit()
	{
		__asm
		{
			call GetTotalNumActiveCopVehicles
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

		MemoryEditor::DigCodeCave(&PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);
		MemoryEditor::DigCodeCave(&PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
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

		for (const auto& pair : pursuitToObserver) 
			pair.second.get()->UpdateOnHeatChange();
	}



	void UpdateState() 
	{
		if (not featureEnabled) return;

		for (const auto& pair : pursuitToObserver) 
			pair.second.get()->UpdateOnGameplay();
	}



	void ResetState()
	{
		if (not featureEnabled) return;
		PursuitFeatures::ResetState();
		CopSpawnOverrides::ResetState();
	}
}