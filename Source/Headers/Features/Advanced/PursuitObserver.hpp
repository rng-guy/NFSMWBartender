#pragma once

#include <vector>
#include <memory>
#include <concepts>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "CopSpawnTables.hpp"
#include "PursuitFeatures.hpp"
#include "CopSpawnOverrides.hpp"
#include "CopFleeOverrides.hpp"
#include "HelicopterOverrides.hpp"
#include "StrategyOverrides.hpp"
#include "LeaderOverrides.hpp"
#include "HeatChangeOverrides.hpp"
#include "RoadblockOverrides.hpp"



namespace PursuitObserver
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[PSO]";
	constexpr Globals::LogLiteral logName = "PursuitObserver";





	// PursuitObserver class ------------------------------------------------------------------------------------------------------------------------

	class PursuitObserver : public PursuitFeatures::Searchable<PursuitObserver>
	{
	private: // aliases

		using CopLabel = PursuitFeatures::Reaction::CopLabel;


	private: // members

		const address pursuit; // pursuit-locked and immobile

		bool firstGameplayUpdatePending    = true;
		bool delayedPursuitUpdatePending   = true;
		bool delayedHeatStateUpdatePending = true;

		ModContainers::StableVector<PursuitFeatures::Reaction> reactions;

		inline static constexpr Globals::LogLiteral name = "PursuitObserver";


	private: // methods

		[[nodiscard]] static CopLabel& GetCopLabelOfVehicle(const address copVehicle)
		{
			const address copAIVehicle = Globals::Vehicle::GetAIVehicle(copVehicle);
			return AsReference<CopLabel>(copAIVehicle - 0x4C + 0x769); // padding byte
		}


		[[nodiscard]] static bool SetCopLabelOfVehicle
		(
			const address  copVehicle, 
			const CopLabel copLabel
		) {
			constexpr CopLabel defaultLabel = static_cast<CopLabel>(0);

			CopLabel& oldLabel = PursuitObserver::GetCopLabelOfVehicle(copVehicle);
			if ((oldLabel == defaultLabel) == (copLabel == defaultLabel)) return false;

			oldLabel = copLabel;

			return true;
		}


		[[nodiscard]] static CopLabel InferCopLabel(const address caller)
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
				Globals::LogWarning(logTag, "Unknown AddVehicle caller:", caller);

			ASSERT_UNREACHABLE_THEN(return CopLabel::UNKNOWN);
		}


		template <class Feature>
		requires std::derived_from<Feature, PursuitFeatures::Reaction>
		void Attach()
		{
			if (not Feature::isEnabled) return;

			this->reactions.Emplace<Feature>(this->pursuit);
		}


	public: // methods

		explicit PursuitObserver(const address pursuit) : pursuit(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogFull("     NEW", logTag, "Pursuit", this->pursuit);

				Globals::LogPlain('+', this, this->name);
			}

			// Container pre-allocations
			this->reactions.Reserve(6);

			// Reaction features
			this->Attach<CopSpawnOverrides  ::ChasersManager>   ();
			this->Attach<CopFleeOverrides   ::MembershipManager>();
			this->Attach<HelicopterOverrides::HelicopterManager>();
			this->Attach<StrategyOverrides  ::StrategyManager>  ();
			this->Attach<LeaderOverrides    ::LeaderManager>    ();
			this->Attach<HeatChangeOverrides::HeatManager>      ();
		}


		PursuitObserver(PursuitObserver&&)      = delete;
		PursuitObserver(const PursuitObserver&) = delete;

		PursuitObserver& operator=(PursuitObserver&&)      = delete;
		PursuitObserver& operator=(const PursuitObserver&) = delete;


		~PursuitObserver()
		{
			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogFull("     DEL", logTag, "Pursuit", this->pursuit);

				Globals::LogPlain('-', this, this->name);
			}
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


		[[nodiscard]] address GetPursuit() const
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
			ASSERT_CONDITION_THEN_IF_FALSE(observer, return);

			const CopLabel newLabel = observer->InferCopLabel(caller);

			if (not PursuitObserver::SetCopLabelOfVehicle(copVehicle, newLabel))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, '=', copVehicle, newLabel, "is already", PursuitObserver::GetCopLabelOfVehicle(copVehicle));

				ASSERT_UNREACHABLE_THEN(return);
			}

			// Process new vehicle
			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, '+', copVehicle, newLabel, Globals::GetVehicleName(copVehicle));

			for (const auto& reaction : observer->reactions)
				reaction->ReactToAddedVehicle(copVehicle, newLabel);
		}


		static void __fastcall ProcessRemovedVehicle
		(
			const address pursuit,
			const address copVehicle
		) {
			auto* const observer = PursuitObserver::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(observer, return);

			const CopLabel oldLabel = PursuitObserver::GetCopLabelOfVehicle(copVehicle);

			if (not PursuitObserver::SetCopLabelOfVehicle(copVehicle, CopLabel::UNKNOWN))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Unknown vehicle", copVehicle, Globals::GetVehicleName(copVehicle), "in", pursuit);

				ASSERT_UNREACHABLE_THEN(return);
			}

			// Process known vehicle
			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, '-', copVehicle, oldLabel, Globals::GetVehicleName(copVehicle));

			for (const auto& reaction : observer->reactions)
				reaction->ReactToRemovedVehicle(copVehicle, oldLabel);
		}
	};



	

	// Feature setup (continued) --------------------------------------------------------------------------------------------------------------------

	// Assembly detours
	RELEASE_CONSTINIT ModContainers::StableVector<PursuitObserver> observers;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall CreateObserver(const address pursuit)
	{
		for (const auto& observer : observers)
		{
			if (observer->GetPursuit() != pursuit) continue; // other pursuit

			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Duplicate pursuit", pursuit);

			ASSERT_UNREACHABLE_THEN(return);
		}

		observers.Emplace(pursuit);
	}



	void NotifyObserversOfHeatStateUpdate()
	{
		for (const auto& observer : observers)
			observer->ProcessHeatStateUpdate();
	}



	void NotifyObserversOfGameplay()
	{
		for (const auto& observer : observers)
			observer->ProcessGameplay();
	}



	void __fastcall DeleteObserver(const address pursuit)
	{
		for (auto it = observers.begin(); it != observers.end(); ++it)
		{
			if ((*it)->GetPursuit() != pursuit) continue; // wrong pursuit

			observers.Erase(it);

			return; // deleted
		}

		if constexpr (Globals::loggingEnabled)
			Globals::LogWarning(logTag, "Unknown pursuit", pursuit);

		ASSERT_UNREACHABLE;
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Notifies pursuit observers of new cop vehicles
	ASSEMBLY_DETOUR(CopAdded, /* begin = */ 0x4338A0, /* end = */ 0x4338A5)
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

			EXIT_ASSEMBLY_DETOUR(CopAdded)
		}
	}



	// Notifies pursuit observers of removed cop vehicles
	ASSEMBLY_DETOUR(CopRemoved, 0x4338B0, 0x4338B5)
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

			EXIT_ASSEMBLY_DETOUR(CopRemoved)
		}
	}



	// Removes observers of deleted pursuits
	ASSEMBLY_DETOUR(PursuitDestructor, 0x433775, 0x43377A)
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

			EXIT_ASSEMBLY_DETOUR(PursuitDestructor)
		}
	}



	// Adds observers for created pursuits
	ASSEMBLY_DETOUR(PursuitConstructor, 0x4432D0, 0x4432D7)
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

			EXIT_ASSEMBLY_DETOUR(PursuitConstructor)
		}
	}



	

	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if (not CopSpawnTables::Initialise(parser)) return false;

		// Initialise sub-features
		CopSpawnOverrides  ::Initialise(parser);
		CopFleeOverrides   ::Initialise(parser);
		LeaderOverrides    ::Initialise(parser);
		StrategyOverrides  ::Initialise(parser);
		HelicopterOverrides::Initialise(parser);
		HeatChangeOverrides::Initialise(parser);
		RoadblockOverrides ::Initialise(parser);

		// Code modifications
		PATCH_ASSEMBLY_DETOUR(CopAdded);
		PATCH_ASSEMBLY_DETOUR(CopRemoved);
		PATCH_ASSEMBLY_DETOUR(PursuitDestructor);
		PATCH_ASSEMBLY_DETOUR(PursuitConstructor);

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

		NotifyObserversOfHeatStateUpdate();
	}



	void NotifyOfGameplay()
	{
		if (not anyFeatureEnabled) return;

		NotifyObserversOfGameplay();
	}



	void NotifyOfSoftEventReset()
	{
		if (not anyFeatureEnabled) return;

		CopSpawnOverrides::NotifyOfSoftEventReset();
	}



	void NotifyOfHardEventReset()
	{
		if (not anyFeatureEnabled) return;

		CopSpawnOverrides::NotifyOfHardEventReset();
	}
}