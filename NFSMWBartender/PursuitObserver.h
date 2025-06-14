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

		bool inPursuitUpdatePending = true;

		std::unordered_set<address>                                       supportCopVehicles;
		std::vector<std::unique_ptr<PursuitFeatures::CopVehicleReaction>> copVehicleReactions;


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
			if (this->IsHelicopter(copVehicle))
				return PursuitFeatures::CopLabel::HELICOPTER;

			else if (this->IsSupport(copVehicle))
				return PursuitFeatures::CopLabel::SUPPORT;

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


		void UpdateOnHeatChange() const
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS: Heat transition");

			for (const auto& reaction : this->copVehicleReactions)
				reaction.get()->UpdateOnHeatChange();
		}


		void UpdateOnGameplay()
		{
			for (const auto& reaction : this->copVehicleReactions)
			{
				if (this->inPursuitUpdatePending)
					reaction.get()->UpdateOnceInPursuit();

				reaction.get()->UpdateOnGameplay();
			}

			this->inPursuitUpdatePending = false;
		}


		void NotifyFeatures
		(
			const address copVehicle,
			const bool    isRemoval
		) {
			static hash (__thiscall* const GetCopType)(address) = (hash (__thiscall*)(address))0x6880A0;

			const PursuitFeatures::CopLabel copLabel = this->Label(copVehicle);
			const hash                      copType  = GetCopType(copVehicle);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "OBS:", (isRemoval) ? '-' : '+', copVehicle, (int)copLabel, this->GetName(copVehicle));

			for (const auto& reaction : this->copVehicleReactions)
			{
				if (isRemoval)
					reaction.get()->ProcessRemoval(copVehicle, copType, copLabel);

				else
					reaction.get()->ProcessAddition(copVehicle, copType, copLabel);
			}

			if (isRemoval) this->supportCopVehicles.erase(copVehicle);
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



	void __fastcall NotifyObserver
	(
		const address pursuit,
		const address copVehicle,
		const bool    isRemoval
	) {
		const auto foundPursuit = pursuitToObserver.find(pursuit);
		if (foundPursuit == pursuitToObserver.end()) return;

		foundPursuit->second.get()->NotifyFeatures(copVehicle, isRemoval);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address pursuitConstructorEntrance = 0x44323D;
	const address pursuitConstructorExit     = 0x443253;

	__declspec(naked) void PursuitConstructor()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0x278], 0x447A0000
			mov dword ptr [esi + 0x268], 0x47C35000
			mov eax, esi

			pushad
			mov ecx, eax
			add ecx, 0x48
			call CreateObserver // ecx: AIPursuit
			popad

			jmp dword ptr pursuitConstructorExit
		}
	}



	const address pursuitDestructorEntrance = 0x433775;
	const address pursuitDestructorExit     = 0x43377A;

	__declspec(naked) void PursuitDestructor()
	{
		__asm
		{
			pushad
			add ecx, 0x48
			call DestroyObserver // ecx: AIPursuit
			popad

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
			pushad
			mov edx, [esp + 0x24]
			push 0x0
			call NotifyObserver // ecx: AIPursuit; edx: PVehicle; stack: isRemoval
			popad

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
			pushad
			mov edx, [esp + 0x24]
			push 0x1
			call NotifyObserver // ecx: AIPursuit; edx: PVehicle; stack: isRemoval
			popad

			// Execute original code and resume
			add ecx, -0x1C
			mov eax, [ecx]

			jmp dword ptr copRemovedExit
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
	}
}