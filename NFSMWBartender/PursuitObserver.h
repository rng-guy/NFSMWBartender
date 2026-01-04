#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <concepts>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "CopSpawnTables.h"
#include "PursuitFeatures.h"
#include "CopSpawnOverrides.h"
#include "CopFleeOverrides.h"
#include "LeaderOverrides.h"
#include "StrategyOverrides.h"
#include "HelicopterOverrides.h"



namespace PursuitObserver
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	HeatParameters::Pair<bool>  passiveHeatGainEnableds(true);
	HeatParameters::Pair<float> heatChangePerAssaults   (0.f);

	HeatParameters::Pair<int> hitCostToHeats       (0);
	HeatParameters::Pair<int> wreckCostToHeats     (0);
	HeatParameters::Pair<int> deploymentCostToHeats(0);
	HeatParameters::Pair<int> insuranceCostToHeats (0);
	HeatParameters::Pair<int> propertyCostToHeats  (0);

	// Code caves
	constexpr float heatScale = 1.f + static_cast<float>(1e-6);

	HashContainers::CachedVaultMap<float> copTypeToHeatChange(0.f);

	size_t lastAnimatedHeatLevel = 0;
	float  animationEndTimestamp = 0.f;





	// PursuitObserver class ------------------------------------------------------------------------------------------------------------------------

	class PursuitObserver
	{
		using PursuitReaction = PursuitFeatures::PursuitReaction;
		using CopLabel        = PursuitReaction::CopLabel;


		// Internal CostTracker class
		class CostTracker
		{
		private:

			int cost = 0;

			const int& count;
			const int  scale;

			const HeatParameters::Pair<int>& costToHeats;



		public:

			explicit CostTracker
			(
				const int&                       count,
				const int                        scale,
				const HeatParameters::Pair<int>& costToHeats
			)
				: count(count), scale(scale), costToHeats(costToHeats)
			{
			}


			explicit CostTracker
			(
				const address                    pursuit,
				const int                        countOffset,
				const int                        scale,
				const HeatParameters::Pair<int>& costToHeats
			)
				: count(*reinterpret_cast<int*>(pursuit + countOffset)), scale(scale), costToHeats(costToHeats)
			{
			}


			float UpdateCost()
			{
				const int newCost = this->scale * this->count;
				const int change  = newCost - this->cost;

				this->cost = newCost;

				if ((change > 0) and (this->costToHeats.current != 0))
					return heatScale * (static_cast<float>(change) / static_cast<float>(this->costToHeats.current));

				return 0.f;
			}


			int GetCost() const
			{
				return this->cost;
			}
		};



	private:

		const address pursuit;

		int   unitDeploymentCost = 0;
		float pendingHeatChange  = 0.f;
		
		bool isFirstGameplayUpdate     = true;
		bool perPursuitUpdatePending   = true;
		bool perHeatLevelUpdatePending = true;

		HashContainers::AddressMap<CopLabel>          copVehicleToLabel;
		std::vector<std::unique_ptr<PursuitReaction>> pursuitReactions;

		const int& pursuitStatus = *reinterpret_cast<int*>(this->pursuit + 0x218);

		CostTracker copsHit         = CostTracker(this->pursuit, 0x15C, 250,  hitCostToHeats);
		CostTracker copsWrecked     = CostTracker(this->pursuit, 0x13C, 5000, wreckCostToHeats);
		CostTracker insuranceClaims = CostTracker(this->pursuit, 0x168, 500,  insuranceCostToHeats);
		CostTracker propertyDamage  = CostTracker(this->pursuit, 0x174, 1,    propertyCostToHeats);

		CostTracker unitDeployment = CostTracker(this->unitDeploymentCost, 1, deploymentCostToHeats);

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


		bool IsSearchModeActive() const
		{
			return (this->pursuitStatus == 2);
		}


		void CheckPursuitCost()
		{
			if (not Globals::playerHeatLevelKnown) return;

			const float previousPendingHeatChange = this->pendingHeatChange;

			this->pendingHeatChange += this->copsHit        .UpdateCost();
			this->pendingHeatChange += this->copsWrecked    .UpdateCost();
			this->pendingHeatChange += this->insuranceClaims.UpdateCost();
			this->pendingHeatChange += this->propertyDamage .UpdateCost();

			static const auto CalculateTotalCost = reinterpret_cast<int (__thiscall*)(address)>(0x40AA00);

			this->unitDeploymentCost = CalculateTotalCost(this->pursuit)
				- this->copsHit        .GetCost() 
				- this->copsWrecked    .GetCost()
				- this->insuranceClaims.GetCost()
				- this->propertyDamage .GetCost();

			this->pendingHeatChange += this->unitDeployment.UpdateCost();

			if (this->IsSearchModeActive())
				this->pendingHeatChange = previousPendingHeatChange;
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
				if (not this->isFirstGameplayUpdate)
				{
					if (this->perPursuitUpdatePending)
						reaction->UpdateOncePerPursuit();

					if (this->perHeatLevelUpdatePending)
						reaction->UpdateOncePerHeatLevel();
				}

				reaction->UpdateOnGameplay();
			}

			if (not this->isFirstGameplayUpdate)
			{
				this->CheckPursuitCost();

				this->perPursuitUpdatePending   = false;
				this->perHeatLevelUpdatePending = false;
			}
			
			this->isFirstGameplayUpdate = false;
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


		static void __stdcall AddToPendingHeatChange
		(
			const address pursuit,
			const float   amount
		) {
			PursuitObserver* const observer = PursuitObserver::FindObserver(pursuit);
	
			if (observer and (not observer->IsSearchModeActive()))
				observer->pendingHeatChange += heatScale * amount;
		}


		static float __fastcall GetPendingHeatChange(const address pursuit) 
		{
			PursuitObserver* const observer = PursuitObserver::FindObserver(pursuit);

			if (observer and (not observer->IsSearchModeActive()))
			{
				const float pendingHeatChange = observer->pendingHeatChange;

				observer->pendingHeatChange = 0.f;

				return pendingHeatChange;
			}

			return 0.f;
		}
	};



	

	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool __stdcall HitTriggersInfraction
	(
		const bool    perpAtFault,
		const address copAIVehicle,
		const address pursuitVehicle,
		const address perpVehicle,
		const address pursuit
	) {
		bool hitTriggersInfraction = false;

		bool& damagedByPerp = *reinterpret_cast<bool*>(pursuitVehicle + 0xB);

		if (not damagedByPerp)
		{
			damagedByPerp = true;

			if (pursuit)
			{
				static const auto NotifyCopDamaged = reinterpret_cast<void(__thiscall*)(address, address)>(0x40AF40);

				hitTriggersInfraction = (perpAtFault and Globals::IsPlayerPursuit(pursuit));

				NotifyCopDamaged(pursuit, copAIVehicle);
			}
		}

		if (perpAtFault and pursuit)
			PursuitObserver::AddToPendingHeatChange(pursuit, heatChangePerAssaults.current);

		return hitTriggersInfraction;
	}



	void __fastcall UpdateHeatAnimation(const address heatMeter)
	{
		const size_t currentHeatLevel = static_cast<size_t>(*reinterpret_cast<float*>(heatMeter + 0x40));

		uint32_t animationScript = 0x1744B3;

		if (currentHeatLevel != lastAnimatedHeatLevel)
		{
			animationScript = 0x41E1FEDC;
			lastAnimatedHeatLevel = currentHeatLevel;
		}

		if (Globals::simulationTime >= animationEndTimestamp)
		{
			static const auto IsScriptSet = reinterpret_cast<bool (__cdecl*)(address, uint32_t)>      (0x514DA0);
			static const auto SetScript   = reinterpret_cast<void (__cdecl*)(address, uint32_t, bool)>(0x514D10);

			const address interfaceObject = *reinterpret_cast<address*>(heatMeter + 0x44);

			if (not IsScriptSet(interfaceObject, animationScript))
			{
				SetScript(interfaceObject, animationScript, true);

				if (animationScript == 0x41E1FEDC)
					animationEndTimestamp = Globals::simulationTime + 2.5f;
			}
		}
	}





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



	constexpr address passiveHeatEntrance = 0x443D4A;
	constexpr address passiveHeatExit     = 0x443D50;

	__declspec(naked) void PassiveHeat()
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x40]
			call PursuitObserver::GetPendingHeatChange // ecx: pursuit
			faddp st(1), st(0)
			fstp dword ptr [esp + 0x1C]

			test ebx, ebx
			je conclusion // no pursuit attributes 

			cmp byte ptr passiveHeatGainEnableds.current, 0x0

			conclusion:
			jmp dword ptr passiveHeatExit
		}
	}



	constexpr address perpCollisionEntrance = 0x429C8B;
	constexpr address perpCollisionExit     = 0x429CBB;

	__declspec(naked) void PerpCollision()
	{
		__asm
		{
			movzx eax, byte ptr [esp + 0x13]

			mov ebx, dword ptr [esp + 0x14]
			mov ecx, dword ptr [esp + 0x18]
			sub ecx, 0x8

			push ebx // pursuit
			push ecx // perpVehicle
			push esi // pursuitVehicle
			push edi // copAIVehicle
			push eax // perpAtFault
			call HitTriggersInfraction
			test al, al

			jmp dword ptr perpCollisionExit
		}
	}



	constexpr address heatMeterResetEntrance = 0x59CEDF;
	constexpr address heatMeterResetExit     = 0x59CEE5;

	__declspec(naked) void HeatMeterReset()
	{
		__asm
		{
			// Execute original code first
			push edi
			xor ebx, ebx
			lea edi, dword ptr [esi + 0x8]

			mov dword ptr lastAnimatedHeatLevel, ebx
			mov dword ptr animationEndTimestamp, ebx

			jmp dword ptr heatMeterResetExit
		}
	}



	constexpr address heatMeterUpdateEntrance = 0x56676D;
	constexpr address heatMeterUpdateExit     = 0x5667C8;

	__declspec(naked) void HeatMeterUpdate()
	{
		__asm
		{
			mov ecx, esi
			call UpdateHeatAnimation // ecx: heatMeter

			jmp dword ptr heatMeterUpdateExit
		}
	}



	constexpr address typeDestructionEntrance = 0x418F99;
	constexpr address typeDestructionExit     = 0x418F9F;

	__declspec(naked) void TypeDestruction()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0xF8], eax

			push eax // copType
			mov ecx, offset copTypeToHeatChange
			call HashContainers::CachedVaultMap<float>::GetValue
			
			push eax // amount
			fstp dword ptr [esp]
			push esi // pursuit
			call PursuitObserver::AddToPendingHeatChange

			jmp dword ptr typeDestructionExit
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

		parser.LoadFile(HeatParameters::configPathAdvanced + "Heat.ini");

		// Heat parameters
		HeatParameters::Parse<bool> (parser, "Heat:Time",    {passiveHeatGainEnableds});
		HeatParameters::Parse<float>(parser, "Heat:Assault", {heatChangePerAssaults});

		HeatParameters::Parse<int, int, int, int, int>
		(
			parser, 
			"Heat:Cost", 
			{hitCostToHeats}, 
			{wreckCostToHeats},
			{deploymentCostToHeats},
			{insuranceCostToHeats},
			{propertyCostToHeats}
		);

		// Destruction Heat gain
		std::vector<std::string> copVehicles;
		std::vector<float>       heatChanges;

		const size_t numCopVehicles = parser.ParseUserParameter("Heat:Wrecking", copVehicles, heatChanges);

		if (numCopVehicles > 0)
		{
			for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
				copTypeToHeatChange.try_emplace(Globals::GetVaultKey(copVehicles[vehicleID].c_str()), heatChanges[vehicleID]);

			// Code modifications (feature-specific)
			MemoryTools::MakeRangeJMP(TypeDestruction, typeDestructionEntrance, typeDestructionExit);
		}

		// Subheaders
		CopSpawnOverrides  ::Initialise(parser);
		CopFleeOverrides   ::Initialise(parser);
		LeaderOverrides    ::Initialise(parser);
		StrategyOverrides  ::Initialise(parser);
		HelicopterOverrides::Initialise(parser);

		// Code modifications (general)
		MemoryTools::MakeRangeNOP(0x429C74, 0x429C7F); // first perp-damage check
	
		MemoryTools::MakeRangeJMP(CopAdded,           copAddedEntrance,           copAddedExit);
		MemoryTools::MakeRangeJMP(CopRemoved,         copRemovedEntrance,         copRemovedExit);
		MemoryTools::MakeRangeJMP(PassiveHeat,        passiveHeatEntrance,        passiveHeatExit);
		MemoryTools::MakeRangeJMP(PerpCollision,      perpCollisionEntrance,      perpCollisionExit);
		MemoryTools::MakeRangeJMP(HeatMeterReset,     heatMeterResetEntrance,     heatMeterResetExit);
		MemoryTools::MakeRangeJMP(HeatMeterUpdate,    heatMeterUpdateEntrance,    heatMeterUpdateExit);
		MemoryTools::MakeRangeJMP(PursuitDestructor,  pursuitDestructorEntrance,  pursuitDestructorExit);
		MemoryTools::MakeRangeJMP(PursuitConstructor, pursuitConstructorEntrance, pursuitConstructorExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if (not copTypeToHeatChange.empty())
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("  CONFIG [OBS] PursuitObserver");

			copTypeToHeatChange.Validate
			(
				"Vehicle-to-change",
				[=](const vault   key) {return Globals::VehicleClassMatches(key, Globals::Class::ANY);},
				[=](const float value) {return true;}
			);
		}

		CopSpawnTables::Validate();

		if constexpr (Globals::loggingEnabled)
			CopSpawnOverrides::LogSetupReport();

		HelicopterOverrides::Validate();
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [OBS] PursuitObserver");

		passiveHeatGainEnableds.Log("passiveHeatGainEnabled  ");
		heatChangePerAssaults  .Log("heatChangePerAssault    ");

		hitCostToHeats       .Log("hitsCostToHeat          ");
		wreckCostToHeats     .Log("wrecksCostToHeat        ");
		deploymentCostToHeats.Log("deploymentCostToHeat    ");
		insuranceCostToHeats .Log("insuranceCostToHeat     ");
		propertyCostToHeats  .Log("propertyCostToHeat      ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		passiveHeatGainEnableds.SetToHeat(isRacing, heatLevel);
		heatChangePerAssaults  .SetToHeat(isRacing, heatLevel);

		hitCostToHeats       .SetToHeat(isRacing, heatLevel);
		wreckCostToHeats     .SetToHeat(isRacing, heatLevel);
		deploymentCostToHeats.SetToHeat(isRacing, heatLevel);
		insuranceCostToHeats .SetToHeat(isRacing, heatLevel);
		propertyCostToHeats  .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();

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