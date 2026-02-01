#pragma once

#include <vector>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace HeatChangeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	HeatParameters::Pair<bool>  passiveHeatGainEnableds(true);
	HeatParameters::Pair<float> heatChangePerAssaults  (0.f);
	HeatParameters::Pair<bool>  onlyOneAssaultPerCops  (true);

	HeatParameters::Pair<int> hitCostToHeats       (0);
	HeatParameters::Pair<int> wreckCostToHeats     (0);
	HeatParameters::Pair<int> deploymentCostToHeats(0);
	HeatParameters::Pair<int> insuranceCostToHeats (0);
	HeatParameters::Pair<int> propertyCostToHeats  (0);

	// Code caves
	constexpr float heatScale = 1.f + static_cast<float>(1e-6);

	HashContainers::CachedCopyVaultMap<float> copTypeToHeatChange(0.f);

	size_t lastAnimatedHeatLevel = 0;
	float  animationEndTimestamp = 0.f;





	// HeatManager class ----------------------------------------------------------------------------------------------------------------------------

	class HeatManager : public PursuitFeatures::PursuitReaction
	{
		// Internal CostTracker class
		class CostTracker
		{
		private:

			int cost = 0;

			const int           scale;
			const volatile int& count;

			const HeatParameters::Pair<int>& costToHeats;



		public:

			explicit CostTracker
			(
				const int& count,
				const int                        scale,
				const HeatParameters::Pair<int>& costToHeats
			)
				: count(count), scale(scale), costToHeats(costToHeats)
			{
			}


			explicit CostTracker
			(
				const address                    pursuit,
				const int                        offset,
				const int                        scale,
				const HeatParameters::Pair<int>& costToHeats
			)
				: count(*reinterpret_cast<volatile int*>(pursuit + offset)), scale(scale), costToHeats(costToHeats)
			{
			}


			explicit CostTracker(const CostTracker&)   = delete;
			CostTracker& operator=(const CostTracker&) = delete;

			explicit CostTracker(CostTracker&&)   = delete;
			CostTracker& operator=(CostTracker&&) = delete;


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

		int   unitDeploymentCost = 0;
		float pendingHeatChange  = 0.f;

		CostTracker copsHit        {this->pursuit, 0x15C, 250,  hitCostToHeats};
		CostTracker copsWrecked    {this->pursuit, 0x13C, 5000, wreckCostToHeats};
		CostTracker insuranceClaims{this->pursuit, 0x168, 500,  insuranceCostToHeats};
		CostTracker propertyDamage {this->pursuit, 0x174, 1,    propertyCostToHeats};

		CostTracker unitDeployment{this->unitDeploymentCost, 1, deploymentCostToHeats};

		inline static HashContainers::AddressMap<HeatManager*> pursuitToManager;


		void CheckPursuitCost()
		{
			if (not Globals::playerHeatLevelKnown) return;

			const float previousPendingHeatChange = this->pendingHeatChange;

			this->pendingHeatChange += this->copsHit        .UpdateCost();
			this->pendingHeatChange += this->copsWrecked    .UpdateCost();
			this->pendingHeatChange += this->insuranceClaims.UpdateCost();
			this->pendingHeatChange += this->propertyDamage .UpdateCost();

			const auto CalculateTotalCost = reinterpret_cast<int (__thiscall*)(address)>(0x40AA00);

			this->unitDeploymentCost = CalculateTotalCost(this->pursuit)
				- this->copsHit        .GetCost()
				- this->copsWrecked    .GetCost()
				- this->insuranceClaims.GetCost()
				- this->propertyDamage .GetCost();

			this->pendingHeatChange += this->unitDeployment.UpdateCost();

			if (Globals::IsInCooldownMode(this->pursuit))
				this->pendingHeatChange = previousPendingHeatChange;
		}


		static HeatManager* FindManager(const address pursuit)
		{
			const auto foundManager = HeatManager::pursuitToManager.find(pursuit);

			if (foundManager != HeatManager::pursuitToManager.end())
				return foundManager->second;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [CNG] No manager for pursuit", pursuit);

			return nullptr;
		}



	public:

		explicit HeatManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "HeatManager");

			this->pursuitToManager.try_emplace(this->pursuit, this);
		}


		~HeatManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "HeatManager");

			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnGameplay() override
		{
			this->CheckPursuitCost();
		}


		static void __stdcall AddToPendingHeatChange
		(
			const address pursuit,
			const float   amount
		) {
			if (Globals::IsInCooldownMode(pursuit)) return;

			HeatManager* const manager = HeatManager::FindManager(pursuit);

			if (manager)
				manager->pendingHeatChange += heatScale * amount;
		}


		static float __fastcall GetPendingHeatChange(const address pursuit)
		{
			if (not Globals::IsInCooldownMode(pursuit))
			{
				HeatManager* const manager = HeatManager::FindManager(pursuit);

				if (manager)
				{
					const float pendingHeatChange = manager->pendingHeatChange;

					manager->pendingHeatChange = 0.f;

					return pendingHeatChange;
				}
			}

			return 0.f;
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address passiveHeatEntrance = 0x443D4A;
	constexpr address passiveHeatExit     = 0x443D50;

	// Adds pending Heat changes from pursuit observers to racer Heat updates
	__declspec(naked) void PassiveHeat()
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x40]
			call HeatManager::GetPendingHeatChange // ecx: pursuit
			faddp st(1), st(0)
			fstp dword ptr [esp + 0x1C]            // new perp Heat

			test ebx, ebx
			je conclusion // no pursuit attributes 

			cmp byte ptr passiveHeatGainEnableds.current, 0x0

			conclusion:
			jmp dword ptr passiveHeatExit
		}
	}



	constexpr address perpCollisionEntrance = 0x429C8B;
	constexpr address perpCollisionExit     = 0x429CBB;

	// Checks whether collisions with cops constitute assault
	__declspec(naked) void PerpCollision()
	{
		__asm
		{
			mov ebx, dword ptr [esp + 0x14]

			test ebx, ebx
			je conclusion // invalid pursuit

			cmp byte ptr [esi + 0xB], 0x1 // damaged by collision with racer
			je fault                      // already damaged

			push edi
			mov ecx, ebx
			mov edx, dword ptr [ebx]
			call dword ptr [edx + 0xF0] // AIPursuit::NotifyCopDamaged

			fault:
			cmp byte ptr [esp + 0x13], 0x0
			je conclusion // perp not at fault

			mov al, byte ptr [esi - 0x758 + 0x76A] // padding byte: assaulted by racer
			test al, byte ptr onlyOneAssaultPerCops.current
			jne infraction                         // only one assault per cop

			push dword ptr heatChangePerAssaults.current // amount
			push ebx                                     // pursuit
			call HeatManager::AddToPendingHeatChange

			infraction:
			mov byte ptr [esi - 0x758 + 0x76A], 0x1 // padding byte: assaulted by racer

			mov ecx, ebx
			call Globals::IsPlayerPursuit
			test al, al

			conclusion:
			mov byte ptr [esi + 0xB], 0x1 // damaged by collision with racer

			jmp dword ptr perpCollisionExit
		}
	}



	constexpr address heatMeterResetEntrance = 0x59CEDF;
	constexpr address heatMeterResetExit     = 0x59CEE5;

	// Prepares the meter state when a new Heat meter is created
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

	// Manages the Heat-transition animation of the Heat meter
	__declspec(naked) void HeatMeterUpdate()
	{
		static constexpr float animationLength = 2.5f; // seconds

		static constexpr address IsFEngScriptSet = 0x514DA0;
		static constexpr address SetFEngScript   = 0x514D10;

		__asm
		{
			push ebx

			mov eax, dword ptr lastAnimatedHeatLevel

			mov ecx, 0x41E1FEDC // Heat-level zoom effect
			mov ebx, 0x1744B3   // seems to be a placeholder

			fld dword ptr [esi + 0x40] // current meter Heat
			fisttp dword ptr lastAnimatedHeatLevel

			cmp eax, dword ptr lastAnimatedHeatLevel
			cmovne ebx, ecx // Heat level changed

			xor cl, cl
			call Globals::GetGameTime // cl: unpaused
			fcom dword ptr animationEndTimestamp
			fnstsw ax
			test ah, 0x5
			jne conclusion            // animation active

			push ebx
			push dword ptr [esi + 0x44]
			call dword ptr IsFEngScriptSet
			add esp, 0x8
			test al, al
			jne conclusion // script already set

			push 0x1
			push ebx
			push dword ptr [esi + 0x44]
			call dword ptr SetFEngScript
			add esp, 0xC

			cmp ebx, 0x41E1FEDC
			jne conclusion // was not animation script

			fadd dword ptr animationLength
			fst dword ptr animationEndTimestamp

			conclusion:
			fstp st(0)
			pop ebx

			jmp dword ptr heatMeterUpdateExit
		}
	}



	constexpr address typeDestructionEntrance = 0x418F99;
	constexpr address typeDestructionExit     = 0x418F9F;

	// Notifies managers of destroyed cop vehicles for Heat-change purposes
	__declspec(naked) void TypeDestruction()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0xF8], eax // last cop type destroyed

			push eax // copType
			mov ecx, offset copTypeToHeatChange
			call HashContainers::CachedCopyVaultMap<float>::GetValue

			push eax // amount
			fstp dword ptr [esp]
			push esi // pursuit
			call HeatManager::AddToPendingHeatChange

			jmp dword ptr typeDestructionExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [CNG] HeatChangeOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Heat.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Heat:Time",    passiveHeatGainEnableds);
		HeatParameters::Parse(parser, "Heat:Assault", heatChangePerAssaults, onlyOneAssaultPerCops);

		HeatParameters::Parse
		(
			parser,
			"Heat:Cost",
			hitCostToHeats,
			wreckCostToHeats,
			deploymentCostToHeats,
			insuranceCostToHeats,
			propertyCostToHeats
		);

		// Destruction Heat gain
		std::vector<std::string> copVehicles;
		std::vector<float>       heatChanges;

		parser.ParseUser<float>("Heat:Wrecking", copVehicles, {heatChanges});

		// Populate destruction Heat-gain map
		const bool mapIsValid = copTypeToHeatChange.FillFromVectors<std::string, float>
		(
			"Vehicle-to-change",
			HeatParameters::defaultValueHandle,
			copVehicles,
			Globals::StringToVaultKey,
			Globals::DoesVehicleExist,
			heatChanges,
			[](const float change) -> float {return change;},
			[](const float change) -> bool  {return true;}
		);

		if (mapIsValid)
		{
			// Code modifications (feature-specific)
			MemoryTools::MakeRangeJMP(TypeDestruction, typeDestructionEntrance, typeDestructionExit);
		}

		// Code modifications (general)
		MemoryTools::MakeRangeNOP(0x429C74, 0x429C7F); // first perp-damage check

		MemoryTools::MakeRangeJMP(PassiveHeat,     passiveHeatEntrance,     passiveHeatExit);
		MemoryTools::MakeRangeJMP(PerpCollision,   perpCollisionEntrance,   perpCollisionExit);
		MemoryTools::MakeRangeJMP(HeatMeterReset,  heatMeterResetEntrance,  heatMeterResetExit);
		MemoryTools::MakeRangeJMP(HeatMeterUpdate, heatMeterUpdateEntrance, heatMeterUpdateExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [CNG] HeatChangeOverrides");

		passiveHeatGainEnableds.Log("passiveHeatGainEnabled  ");
		heatChangePerAssaults  .Log("heatChangePerAssault    ");
		onlyOneAssaultPerCops  .Log("onlyOneAssaultPerCop    ");

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
		onlyOneAssaultPerCops  .SetToHeat(isRacing, heatLevel);

		hitCostToHeats       .SetToHeat(isRacing, heatLevel);
		wreckCostToHeats     .SetToHeat(isRacing, heatLevel);
		deploymentCostToHeats.SetToHeat(isRacing, heatLevel);
		insuranceCostToHeats .SetToHeat(isRacing, heatLevel);
		propertyCostToHeats  .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}