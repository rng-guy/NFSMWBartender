#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace HeatChangeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Value<bool> heatTimerEnabled(true);

	constinit HeatParameters::Value<float> chaserHeatChange    (0.f); // levels
	constinit HeatParameters::Value<float> supportHeatChange   (0.f); // levels
	constinit HeatParameters::Value<float> helicopterHeatChange(0.f); // levels

	constinit HeatParameters::Value<float> roadblockHeatChange(0.f); // levels
	constinit HeatParameters::Value<float> spikesHeatChange   (0.f); // levels

	constinit HeatParameters::Value<float> copWreckHeatChange(0.f); // levels

	constinit HeatParameters::Value<float> copDamagedHeatChange(0.f); // levels
	constinit HeatParameters::Value<float> trafficHitHeatChange(0.f); // levels

	constinit HeatParameters::Value<float> heatChangePerAssault(0.f); // levels
	constinit HeatParameters::Value<bool>  onlyOneAssaultPerCop(true);

	constinit HeatParameters::Value<float> damageHeatChange(0.f); // levels

	// Code caves
	size_t lastAnimatedHeatLevel = 0;
	float  animationEndTimestamp = 0.f;

	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<float> copTypeToWreckHeatChange(0.f); // levels





	// HeatManager class ----------------------------------------------------------------------------------------------------------------------------

	class HeatManager : public PursuitFeatures::PursuitReaction
	{
	private:

		// Tracker for event-based Heat changes
		class CountTracker
		{
		private:

			int lastCount = 0;

			const int&                          count;
			const HeatParameters::Value<float>& heatPerCount;


		public:

			explicit CountTracker
			(
				const address                       pursuit,
				const ptrdiff_t                     offset,
				const HeatParameters::Value<float>& heatPerCount
			)
				: count(AsReference<int>(pursuit + offset)), heatPerCount(heatPerCount)
			{
			}


			explicit CountTracker
			(
				const address, 
				const ptrdiff_t,
				const HeatParameters::Value<float>&&
			) 
				= delete;


			explicit CountTracker(CountTracker&&)      = delete;
			explicit CountTracker(const CountTracker&) = delete;

			CountTracker& operator=(CountTracker&&)      = delete;
			CountTracker& operator=(const CountTracker&) = delete;


			[[nodiscard]] float YieldHeatChange()
			{
				const int change = this->count - this->lastCount;

				this->lastCount += change;
				if (change <= 0) return 0.f;

				return static_cast<float>(change) * this->heatPerCount.current;
			}
		};


	private:

		float pendingHeatChange = 0.f;

		std::array<CountTracker, 6> automaticTrackers =
		{
			CountTracker{this->pursuit, 0x184, chaserHeatChange},
			CountTracker{this->pursuit, 0x188, supportHeatChange},
			CountTracker{this->pursuit, 0x150, helicopterHeatChange},

			CountTracker{this->pursuit, 0x158, roadblockHeatChange},
			CountTracker{this->pursuit, 0x17C, spikesHeatChange},

			CountTracker{this->pursuit, 0x168, trafficHitHeatChange}
		};
	
		inline static RELEASE_CONSTINIT ModContainers::AddressMap<HeatManager*> pursuitToManager;


		void AddToPendingHeatChange(const float amount)
		{
			if (Globals::IsPursuitInCooldownMode(this->pursuit)) return;

			this->pendingHeatChange += Globals::floatScale * amount;
		}


		void ProcessTrackerYields()
		{
			if (not Globals::playerHeatLevelKnown) return;

			float totalHeatChange = 0.f;

			for (CountTracker& tracker : this->automaticTrackers)
				totalHeatChange += tracker.YieldHeatChange();

			this->AddToPendingHeatChange(totalHeatChange);
		}


		[[nodiscard]] static HeatManager* FindManager(const address pursuit)
		{
			const auto foundManager = HeatManager::pursuitToManager.find(pursuit);

			if (foundManager != HeatManager::pursuitToManager.end())
				return foundManager->second;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [CNG] No manager for pursuit", pursuit);

			return nullptr; // should never happen
		}


	public:

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


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


		void ReactToGameplay() override
		{
			this->ProcessTrackerYields();
		}


		static void NotifyOfDamagedCop(const address pursuit) 
		{
			auto* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->AddToPendingHeatChange(copDamagedHeatChange.current);
		}


		static void NotifyOfAssaultedCop
		(
			const address pursuit,
			const bool    isFirstOffence
		) {
			if ((not isFirstOffence) and onlyOneAssaultPerCop.current) return;

			auto* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->AddToPendingHeatChange(heatChangePerAssault.current);
		}


		static void __fastcall NotifyOfPropertyDamage
		(
			const address pursuit,
			const int     damageAmount
		) {
			auto* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->AddToPendingHeatChange(static_cast<float>(damageAmount) * damageHeatChange.current);
		}


		static void NotifyOfDestroyedCop
		(
			const address pursuit,
			const address copVehicle
		) {
			auto* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			const vault copType        = Globals::GetVehicleType(copVehicle);
			const float typeWreckBonus = copTypeToWreckHeatChange.GetValue(copType);

			manager->AddToPendingHeatChange(copWreckHeatChange.current + typeWreckBonus);
		}


		[[nodiscard]] static float __fastcall YieldPendingHeatChange(const address pursuit)
		{
			if (Globals::IsPursuitInCooldownMode(pursuit)) return 0.f;

			auto* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return 0.f; // should never happen

			const float heatChange = manager->pendingHeatChange;

			if constexpr (Globals::loggingEnabled)
			{
				if (heatChange != 0.f)
					Globals::logger.Log(pursuit, "[CNG] Heat change:", heatChange);
			}

			manager->pendingHeatChange = 0.f;

			return heatChange;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall ClampHeatLimits(const address pursuit)
	{
		float& minHeat = AsReference<float>(pursuit + 0x9C);
		float& maxHeat = AsReference<float>(pursuit + 0xA0);

		minHeat = std::clamp<float>(minHeat, 1.f, HeatParameters::maxHeat);
		maxHeat = std::clamp<float>(maxHeat, 1.f, HeatParameters::maxHeat);

		if (minHeat > maxHeat)
			minHeat = maxHeat;
	}



	void __fastcall UpdateHeatAnimation(const address heatMeter)
	{
		const float  totalGameTime    = Globals::GetTotalGameTime();
		const size_t currentHeatLevel = static_cast<size_t>(AsReference<float>(heatMeter + 0x40));
		
		if (totalGameTime >= animationEndTimestamp)
		{
			const auto IsFEngScriptSet = AsFunction<bool __cdecl (address, uint32_t)>      (0x514DA0);
			const auto SetFEngScript   = AsFunction<void __cdecl (address, uint32_t, bool)>(0x514D10);

			const address  interfaceObject = AsReference<address>(heatMeter + 0x44);
			const uint32_t animationScript = (currentHeatLevel != lastAnimatedHeatLevel) ? 0x41E1FEDC : 0x1744B3;

			if (not IsFEngScriptSet(interfaceObject, animationScript))
			{
				SetFEngScript(interfaceObject, animationScript, true);

				if (animationScript == 0x41E1FEDC)
					animationEndTimestamp = totalGameTime + 2.5f; // animation length (seconds)
			}
		}

		lastAnimatedHeatLevel = currentHeatLevel;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatLimitsEntrance = 0x443171;
	constexpr address heatLimitsExit     = 0x44317A;

	// Ensures the Heat limits of pursuits are valid
	__declspec(naked) void HeatLimits()
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x48]
			call ClampHeatLimits // ecx: pursuit

			// Execute original code and resume
			push ebx
			mov ecx, esi
			mov dword ptr [esi + 0xE0], ebx
			
			jmp dword ptr [heatLimitsExit]
		}
	}



	constexpr address passiveHeatEntrance = 0x443D4A;
	constexpr address passiveHeatExit     = 0x443D50;

	// Adds pending Heat changes from pursuit observers to racer Heat updates
	__declspec(naked) void PassiveHeat()
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x40]
			call HeatManager::YieldPendingHeatChange // ecx: pursuit
			faddp st(1), st(0)
			fstp dword ptr [esp + 0x1C]              // new perp Heat

			test ebx, ebx
			je conclusion // no pursuit attributes 

			cmp byte ptr [heatTimerEnabled.current], 0

			conclusion:
			jmp dword ptr [passiveHeatExit]
		}
	}



	constexpr address spikeCounterEntrance = 0x43E654;
	constexpr address spikeCounterExit     = 0x43E663;

	// Increments the "spikes deployed" counter correctly
	__declspec(naked) void SpikeCounter()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x10]
			cmp dword ptr [ecx], 3 // prop ID
			jne conclusion         // prop not spike strip

			mov edx, dword ptr [esp + 0x4C4] // roadblock pursuit
			inc dword ptr [edx + 0x17C]      // spike strips deployed

			conclusion:
			jmp dword ptr [spikeCounterExit]
		}
	}



	constexpr address supportCheckEntrance = 0x423FA2;
	constexpr address supportCheckExit     = 0x423FF1;

	// Checks for support cops to increment deployment counter
	__declspec(naked) void SupportCheck()
	{
		static constexpr address IsSupportVehicle = 0x419890;

		__asm
		{
			// Execute original code first
			mov ebx, eax
			add esp, 0x4

			push edi // copVehicle
			lea ecx, dword ptr [esi + 0x10]
			call dword ptr [IsSupportVehicle]
			cmp al, 1

			jmp dword ptr [supportCheckExit]
		}
	}



	constexpr address propertyDamageEntrance = 0x40945A;
	constexpr address propertyDamageExit     = 0x409461;

	// Notifies HeatManager of incurred property damage
	__declspec(naked) void PropertyDamage()
	{
		__asm
		{
			push edx

			mov ecx, esi
			mov edx, dword ptr [esp + 0x14]
			call HeatManager::NotifyOfPropertyDamage // ecx: pursuit; edx: damageAmount

			pop edx

			// Execute original code and resume
			cmp dword ptr [edx + 0x1964], 2

			jmp dword ptr [propertyDamageExit]
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

			mov dword ptr [lastAnimatedHeatLevel], ebx
			mov dword ptr [animationEndTimestamp], ebx

			jmp dword ptr [heatMeterResetExit]
		}
	}



	constexpr address heatMeterUpdateEntrance = 0x56676D;
	constexpr address heatMeterUpdateExit     = 0x5667C8;

	// Manages the Heat-transition animation of the Heat meter
	__declspec(naked) void HeatMeterUpdate()
	{
		__asm
		{
			mov ecx, esi
			call UpdateHeatAnimation // ecx: heatMeter

			jmp dword ptr [heatMeterUpdateExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ParseDamageChanges(const HeatParameters::Parser& parser)
	{
		HeatParameters::Value<int> damageToHeat(0);
		HeatParameters::Parse(parser, "Heat:Damage", damageToHeat);

		for (const bool forRaces : {false, true})
		{
			const auto& damageArray = damageToHeat    .GetHeatLevelArray(forRaces);
			auto&       changeArray = damageHeatChange.GetHeatLevelArray(forRaces);

			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				const int damage = damageArray[heatLevelID];

				if (damage != 0)
					changeArray[heatLevelID] = 1.f / static_cast<float>(damage);
			}
		}
	}



	bool ParseVehicleWreckChanges(const HeatParameters::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<float>            heatChanges;

		parser.ParseUser<std::string_view, float>("Wrecking:Vehicles", copNames, {heatChanges});

		return copTypeToWreckHeatChange.FillFromVectors
		(
			"Vehicle-to-change",
			HeatParameters::configDefaultVaultHash,
			ModContainers::MapFillSetup(copNames,    Globals::GetVaultHash, Globals::DoesVehicleTypeExist),
			ModContainers::MapFillSetup(heatChanges, std::identity{},       ModContainers::AlwaysValid{})
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [CNG] HeatChangeOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Heat.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Heat:Time", heatTimerEnabled);

		HeatParameters::Parse(parser, "Heat:Deployment", chaserHeatChange, supportHeatChange, helicopterHeatChange);

		HeatParameters::Parse(parser, "Heat:Roadblocks", roadblockHeatChange, spikesHeatChange);

		HeatParameters::Parse(parser, "Heat:Wrecking", copWreckHeatChange);

		HeatParameters::Parse(parser, "Heat:Collisions", copDamagedHeatChange, trafficHitHeatChange);

		HeatParameters::Parse(parser, "Collisions:Assault", heatChangePerAssault, onlyOneAssaultPerCop);

		ParseDamageChanges(parser);

		// Vehicle-specific Heat changes
		ParseVehicleWreckChanges(parser);

		// Code modifications (general)
		MemoryTools::Write<byte>(0xEB, {0x44307F}); // Heat limits in Challenge Series events

		MemoryTools::MakeRangeJMP<heatLimitsEntrance,      heatLimitsExit>     (HeatLimits);
		MemoryTools::MakeRangeJMP<passiveHeatEntrance,     passiveHeatExit>    (PassiveHeat);
		MemoryTools::MakeRangeJMP<spikeCounterEntrance,    spikeCounterExit>   (SpikeCounter);
		MemoryTools::MakeRangeJMP<supportCheckEntrance,    supportCheckExit>   (SupportCheck);
		MemoryTools::MakeRangeJMP<propertyDamageEntrance,  propertyDamageExit> (PropertyDamage);
		MemoryTools::MakeRangeJMP<heatMeterResetEntrance,  heatMeterResetExit> (HeatMeterReset);
		MemoryTools::MakeRangeJMP<heatMeterUpdateEntrance, heatMeterUpdateExit>(HeatMeterUpdate);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		Globals::logger.Log("    HEAT [CNG] HeatChangeOverrides");

		heatTimerEnabled.Log("heatTimerEnabled        ");

		chaserHeatChange    .Log("chaserHeatChange        ");
		supportHeatChange   .Log("supportHeatChange       ");
		helicopterHeatChange.Log("helicopterHeatChange    ");

		roadblockHeatChange.Log("roadblockHeatChange     ");
		spikesHeatChange   .Log("spikesHeatChange        ");

		copWreckHeatChange.Log("copWreckHeatChange      ");

		copDamagedHeatChange.Log("copDamagedHeatChange    ");
		trafficHitHeatChange.Log("trafficHitHeatChange    ");

		heatChangePerAssault.Log("heatChangePerAssault    ");
		onlyOneAssaultPerCop.Log("onlyOneAssaultPerCop    ");

		damageHeatChange.Log("damageHeatChange        ");
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		heatTimerEnabled.SetToHeatState(isRacing, heatLevel);

		chaserHeatChange    .SetToHeatState(isRacing, heatLevel);
		supportHeatChange   .SetToHeatState(isRacing, heatLevel);
		helicopterHeatChange.SetToHeatState(isRacing, heatLevel);

		roadblockHeatChange.SetToHeatState(isRacing, heatLevel);
		spikesHeatChange   .SetToHeatState(isRacing, heatLevel);

		copWreckHeatChange.SetToHeatState(isRacing, heatLevel);

		copDamagedHeatChange.SetToHeatState(isRacing, heatLevel);
		trafficHitHeatChange.SetToHeatState(isRacing, heatLevel);

		heatChangePerAssault.SetToHeatState(isRacing, heatLevel);
		onlyOneAssaultPerCop.SetToHeatState(isRacing, heatLevel);

		damageHeatChange.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}



	void NotifyOfDamagedCop
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle
	) {
		if (not anyFeatureEnabled) return;

		HeatManager::NotifyOfDamagedCop(pursuit);
	}



	void NotifyOfAssaultedCop
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle,
		const bool    isFirstOffence
	) {
		if (not anyFeatureEnabled) return;

		HeatManager::NotifyOfAssaultedCop(pursuit, isFirstOffence);
	}



	void NotifyOfDestroyedCop
	(
		const address pursuit, 
		const address copVehicle
	) {
		if (not anyFeatureEnabled) return;

		HeatManager::NotifyOfDestroyedCop(pursuit, copVehicle);
	}
}