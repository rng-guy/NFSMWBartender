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

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;





	// PursuitObserver class ------------------------------------------------------------------------------------------------------------------------

	class PursuitObserver
	{
		using PursuitReaction = PursuitFeatures::PursuitReaction;
		using CopLabel        = PursuitReaction::CopLabel;



	private:

		const address pursuit;

		bool perPursuitUpdatePending   = true;
		bool perHeatLevelUpdatePending = true;

		HashContainers::AddressMap<CopLabel>          copVehicleToLabel;
		std::vector<std::unique_ptr<PursuitReaction>> pursuitReactions;
		
		inline static HashContainers::SafeAddressMap<PursuitObserver> pursuitToObserver;


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


		static PursuitObserver* FindObserver(const address pursuit)
		{
			const auto foundObserver = PursuitObserver::pursuitToObserver.find(pursuit);

			if (foundObserver != PursuitObserver::pursuitToObserver.end())
				return foundObserver->second.get();

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] No observer for pursuit", pursuit);

			return nullptr;
		}



	public:

		explicit PursuitObserver(const address pursuit) : pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "PursuitObserver");

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
			PursuitObserver* const observer = PursuitObserver::FindObserver(pursuit);
			if (not observer) return; // should never happen

			const CopLabel copLabel         = observer->LabelAddVehicleCall(caller);
			const auto     [pair, wasAdded] = observer->copVehicleToLabel.try_emplace(copVehicle, copLabel);

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
			PursuitObserver* const observer = PursuitObserver::FindObserver(pursuit);
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
	};



	

	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

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
			sub ecx, 0x1C
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
			sub ecx, 0x1C
			mov eax, dword ptr [ecx]

			jmp dword ptr copRemovedExit
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
		
		// Code modifications 
		MemoryTools::MakeRangeJMP(CopAdded,           copAddedEntrance,           copAddedExit);
		MemoryTools::MakeRangeJMP(CopRemoved,         copRemovedEntrance,         copRemovedExit);
		MemoryTools::MakeRangeJMP(PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
		MemoryTools::MakeRangeJMP(PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		CopSpawnTables::Validate();

		if constexpr (Globals::loggingEnabled)
			CopSpawnOverrides::LogSetupReport();

		HelicopterOverrides::Validate();
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

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
}