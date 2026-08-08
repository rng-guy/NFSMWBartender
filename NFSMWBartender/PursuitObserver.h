#pragma once

#include <vector>
#include <memory>
#include <utility>
#include <concepts>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"

#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "CopSpawnOverrides.h"
#include "CopFleeOverrides.h"
#include "HelicopterOverrides.h"
#include "StrategyOverrides.h"
#include "LeaderOverrides.h"
#include "HeatChangeOverrides.h"
#include "RoadblockOverrides.h"



namespace PursuitObserver
{
	// PursuitObserver class ------------------------------------------------------------------------------------------------------------------------

	class PursuitObserver : public PursuitFeatures::Searchable<PursuitObserver>
	{
	private: // aliases

		using CopLabel = PursuitFeatures::Reaction::CopLabel;


	private: // members

		const address pursuit;

		bool firstGameplayUpdatePending    = true;
		bool delayedPursuitUpdatePending   = true;
		bool delayedHeatStateUpdatePending = true;

		ModContainers::AddressMap<CopLabel> copVehicleToLabel;

		ModContainers::PointerStorage<PursuitFeatures::Reaction> reactions;


	private: // methods

		[[nodiscard]] static CopLabel InferCopLabelFromCaller(const address caller)
		{
			switch (caller)
			{
			case 0x40B02A: // roadblock cop after spike-strip hit
			case 0x4443D8: // regular roadblock cop
				return CopLabel::ROADBLOCK;

			case 0x41F7E6: // LeaderStrategy spawn
				return CopLabel::LEADER;

			case 0x41F426: // HeavyStrategy 3 spawn
				return CopLabel::HEAVY;

			case 0x426BC6: // helicopter
				return CopLabel::HELICOPTER;

			case 0x43EAF5: // free patrol
			case 0x43EE97: // first patrol in race
			case 0x42E872: // scripted event spawn
			case 0x42EB73: // first cop of milestone pursuit
			case 0x4311EC: // regular pursuit spawn
				return CopLabel::CHASER;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Unknown AddVehicle caller:", caller);

			return CopLabel::UNKNOWN;
		}


		template <class Feature>
		requires std::derived_from<Feature, PursuitFeatures::Reaction>
		void AttachReaction()
		{
			if (not Feature::isEnabled) return;

			this->reactions.EmplaceObject<Feature>(this->pursuit);
		}


	public: // methods

		explicit PursuitObserver(const address pursuit) : pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "PursuitObserver");

			// Container pre-allocations
			this->reactions        .ReserveCapacity(6);
			this->copVehicleToLabel.reserve        (100);

			// Reaction features
			this->AttachReaction<CopSpawnOverrides  ::ChasersManager>   ();
			this->AttachReaction<CopFleeOverrides   ::MembershipManager>();
			this->AttachReaction<HelicopterOverrides::HelicopterManager>();
			this->AttachReaction<StrategyOverrides  ::StrategyManager>  ();
			this->AttachReaction<LeaderOverrides    ::LeaderManager>    ();
			this->AttachReaction<HeatChangeOverrides::HeatManager>      ();
		}


		explicit PursuitObserver(PursuitObserver&&)      = delete;
		explicit PursuitObserver(const PursuitObserver&) = delete;

		PursuitObserver& operator=(PursuitObserver&&)      = delete;
		PursuitObserver& operator=(const PursuitObserver&) = delete;


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "PursuitObserver");
		}


		void ProcessHeatStateUpdate()
		{
			for (const auto& reaction : this->reactions)
				reaction->ReactToHeatStateUpdate();

			this->delayedHeatStateUpdatePending = true;
		}


		void ProcessGameplay()
		{
			for (const auto& reaction : this->reactions)
			{
				if (not this->firstGameplayUpdatePending)
				{
					if (this->delayedPursuitUpdatePending)
						reaction->ReactToPursuitStartWithDelay();

					if (this->delayedHeatStateUpdatePending)
						reaction->ReactToHeatStateUpdateWithDelay();
				}

				reaction->ReactToGameplay();
			}

			if (not this->firstGameplayUpdatePending)
			{
				this->delayedPursuitUpdatePending   = false;
				this->delayedHeatStateUpdatePending = false;
			}
			else this->firstGameplayUpdatePending = false;
		}


		address GetPursuit() const
		{
			return this->pursuit;
		}


		static void __stdcall ProcessAddedVehicle
		(
			const address pursuit,
			const address copVehicle,
			const address caller
		) {
			auto* const observer = PursuitObserver::FindInstance(pursuit);
			if (not observer) return; // should never happen

			const CopLabel copLabel               = observer->InferCopLabelFromCaller(caller);
			const auto     [pairIt, isNewVehicle] = observer->copVehicleToLabel.try_emplace(copVehicle, copLabel);
			
			if (isNewVehicle)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[OBS] +", copVehicle, copLabel, Globals::GetVehicleName(copVehicle));

				for (const auto& reaction : observer->reactions)
					reaction->ReactToAddedVehicle(copVehicle, copLabel);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[OBS] =", copVehicle, copLabel, "is already", pairIt->second);
		}


		static void __fastcall ProcessRemovedVehicle
		(
			const address pursuit,
			const address copVehicle
		) {
			auto* const observer = PursuitObserver::FindInstance(pursuit);
			if (not observer) return; // should never happen

			const auto foundVehicle = observer->copVehicleToLabel.find(copVehicle);

			if (foundVehicle != observer->copVehicleToLabel.end())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[OBS] -", copVehicle, foundVehicle->second, Globals::GetVehicleName(copVehicle));

				for (const auto& reaction : observer->reactions)
					reaction->ReactToRemovedVehicle(copVehicle, foundVehicle->second);

				observer->copVehicleToLabel.erase(foundVehicle);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Unknown vehicle", copVehicle, Globals::GetVehicleName(copVehicle), "in", pursuit);
		}
	};



	

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Code caves
	RELEASE_CONSTINIT ModContainers::PointerStorage<PursuitObserver> observers;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall CreateObserver(const address pursuit)
	{
		for (const auto& observer : observers)
		{
			if (observer->GetPursuit() != pursuit) continue; // other pursuit

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [OBS] Duplicate pursuit", pursuit);

			return; // should never happen
		}

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("     NEW [OBS] Pursuit", pursuit);

		observers.EmplaceObject(pursuit);
	}



	void NotifyOfHeatStateUpdate()
	{
		for (const auto& observer : observers)
			observer->ProcessHeatStateUpdate();
	}



	void NotifyOfGameplay()
	{
		for (const auto& observer : observers)
			observer->ProcessGameplay();
	}



	void __fastcall DeleteObserver(const address pursuit)
	{
		for (auto it = observers.begin(); it != observers.end(); ++it)
		{
			if ((*it)->GetPursuit() != pursuit) continue; // wrong pursuit

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("     DEL [OBS] Pursuit", pursuit);

			observers.EraseObject(it);

			return; // deleted
		}

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [OBS] Unknown pursuit", pursuit);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copAddedEntrance = 0x4338A0;
	constexpr address copAddedExit     = 0x4338A5;

	// Notifies pursuit observers of new cop vehicles
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

			jmp dword ptr [copAddedExit]
		}
	}



	constexpr address copRemovedEntrance = 0x4338B0;
	constexpr address copRemovedExit     = 0x4338B5;

	// Notifies pursuit observers of removed cop vehicles
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

			jmp dword ptr [copRemovedExit]
		}
	}



	constexpr address pursuitDestructorEntrance = 0x433775;
	constexpr address pursuitDestructorExit     = 0x43377A;

	// Removes observers of deleted pursuits
	__declspec(naked) void PursuitDestructor()
	{
		__asm
		{
			push ecx

			add ecx, 0x48
			call DeleteObserver // ecx: pursuit

			pop ecx

			// Execute original code and resume
			sub esp, 0x8
			push ebx
			push esi

			jmp dword ptr [pursuitDestructorExit]
		}
	}



	constexpr address pursuitConstructorEntrance = 0x4432D0;
	constexpr address pursuitConstructorExit     = 0x4432D7;

	// Adds observers for created pursuits
	__declspec(naked) void PursuitConstructor()
	{
		__asm
		{
			add eax, 0x2C
			push eax

			lea ecx, dword ptr [eax + 0x1C]
			call CreateObserver // ecx: pursuit

			pop eax

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x8]

			jmp dword ptr [pursuitConstructorExit]
		}
	}



	

	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if (not CopSpawnTables::InitialiseFeatures(parser)) return false;

		// Initialise sub-features
		CopSpawnOverrides  ::InitialiseFeatures(parser);
		CopFleeOverrides   ::InitialiseFeatures(parser);
		LeaderOverrides    ::InitialiseFeatures(parser);
		StrategyOverrides  ::InitialiseFeatures(parser);
		HelicopterOverrides::InitialiseFeatures(parser);
		HeatChangeOverrides::InitialiseFeatures(parser);
		RoadblockOverrides ::InitialiseFeatures(parser);

		// Code modifications
		MemoryTools::MakeRangeJMP<copAddedEntrance,           copAddedExit>          (CopAdded);
		MemoryTools::MakeRangeJMP<copRemovedEntrance,         copRemovedExit>        (CopRemoved);
		MemoryTools::MakeRangeJMP<pursuitDestructorEntrance,  pursuitDestructorExit> (PursuitDestructor);
		MemoryTools::MakeRangeJMP<pursuitConstructorEntrance, pursuitConstructorExit>(PursuitConstructor);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		CopSpawnTables     ::SetToHeatState(state);
		CopSpawnOverrides  ::SetToHeatState(state);
		CopFleeOverrides   ::SetToHeatState(state);
		HelicopterOverrides::SetToHeatState(state);
		StrategyOverrides  ::SetToHeatState(state);
		LeaderOverrides    ::SetToHeatState(state);
		HeatChangeOverrides::SetToHeatState(state);
		RoadblockOverrides ::SetToHeatState(state);

		NotifyOfHeatStateUpdate();
	}



	void UpdateFeatureState() 
	{
		if (not anyFeatureEnabled) return;

		NotifyOfGameplay();
	}
}