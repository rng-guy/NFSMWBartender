#pragma once

#include <array>
#include <cstdint>
#include <algorithm>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ParameterSets.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "PursuitFeatures.hpp"



namespace HeatChangeOverrides
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[CNG]";
	constexpr Globals::LogLiteral logName = "HeatChangeOverrides";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, heatTimerEnabled, true);

	constinit HEAT_PARAMETER_VALUE(float, challengeScale, 600.f, {0.f}); // seconds

	constinit HEAT_PARAMETER_VALUE(float, chaserHeatChange,     0.f); // levels
	constinit HEAT_PARAMETER_VALUE(float, supportHeatChange,    0.f); // levels
	constinit HEAT_PARAMETER_VALUE(float, helicopterHeatChange, 0.f); // levels

	constinit HEAT_PARAMETER_VALUE(float, roadblockHeatChange, 0.f); // levels
	constinit HEAT_PARAMETER_VALUE(float, spikesHeatChange,    0.f); // levels

	constinit HEAT_PARAMETER_VALUE(float, trafficHitHeatChange, 0.f); // levels

	constinit HEAT_PARAMETER_VALUE(float, propertyHeatChange, 0.f); // levels

	// Parameter sets
	RELEASE_CONSTINIT ParameterSets::CopInteractions heatInteractions; // levels

	// Assembly detours
	size_t lastAnimatedHeatLevel = 0;
	float  animationEndTimestamp = 0.f;





	// HeatManager class ----------------------------------------------------------------------------------------------------------------------------

	class HeatManager : public PursuitFeatures::Reaction, public PursuitFeatures::Searchable<HeatManager>
	{
	private: // types

		class CountTracker
		{
		private: // members

			int lastCount = 0;

			const int&                          count;        // pursuit-locked (through count) and immobile
			const HeatParameters::Value<float>& heatPerCount; // count-locked upon construction


		public: // methods

			CountTracker
			(
				const address                       pursuit,
				const ptrdiff_t                     offset,
				const HeatParameters::Value<float>& heatPerCount
			)
				: count(AsReference<int>(pursuit + offset)), heatPerCount(heatPerCount)
			{
			}


			CountTracker(CountTracker&&)      = delete;
			CountTracker(const CountTracker&) = delete;

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


	private: // members

		float pendingHeatChange = 0.f;

		std::array<CountTracker, 6> countTrackers =
		{
			CountTracker(this->pursuit, 0x184, chaserHeatChange),
			CountTracker(this->pursuit, 0x188, supportHeatChange),
			CountTracker(this->pursuit, 0x150, helicopterHeatChange),
			CountTracker(this->pursuit, 0x158, roadblockHeatChange),
			CountTracker(this->pursuit, 0x17C, spikesHeatChange),
			CountTracker(this->pursuit, 0x168, trafficHitHeatChange)
		};

		inline static constexpr Globals::LogLiteral name = "HeatManager";
	

	private: // methods

		void AddToPendingHeatChange(const float levels)
		{
			if (Globals::IsPursuitInCooldownMode(this->pursuit)) return;

			this->pendingHeatChange += levels;
		}


		void ProcessTrackerYields()
		{
			float totalHeatChange = 0.f;

			for (CountTracker& tracker : this->countTrackers)
				totalHeatChange += tracker.YieldHeatChange();

			this->AddToPendingHeatChange(totalHeatChange);
		}


		[[nodiscard]] static HeatManager* FindInstanceByPerpVehicle(const address perpVehicle)
		{
			return HeatManager::FindInstance(Globals::GetPursuitOfPerpVehicle(perpVehicle));
		}


	public: // members

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


	public: // methods

		explicit HeatManager(const address pursuit) : PursuitFeatures::Reaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('+', this, this->name);
		}


		~HeatManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('-', this, this->name);
		}


		void ReactToGameplay() override
		{
			this->ProcessTrackerYields();
		}


		static void ProcessTaggedCop
		(
			const address copVehicle, 
			const address perpVehicle
		) {
			auto* const manager = HeatManager::FindInstanceByPerpVehicle(perpVehicle);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			const float heatChange = heatInteractions.GetTaggingChange(copVehicle);
			manager->AddToPendingHeatChange(heatChange);
		}


		static void ProcessAssaultedCop
		(
			const address copVehicle,
			const address perpVehicle,
			const byte    numCopAssaulted
		) {
			auto* const manager = HeatManager::FindInstanceByPerpVehicle(perpVehicle);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			const float heatChange = heatInteractions.GetAssaultChange(copVehicle, numCopAssaulted);
			manager->AddToPendingHeatChange(heatChange);
		}


		static void __fastcall ProcessPropertyDamage
		(
			const address pursuit,
			const int     damageAmount
		) {
			auto* const manager = HeatManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			const float heatChange = static_cast<float>(damageAmount) * propertyHeatChange.current;
			manager->AddToPendingHeatChange(heatChange);
		}


		static void ProcessDestroyedCop
		(
			const address pursuit,
			const address copVehicle
		) {
			auto* const manager = HeatManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			const float heatChange = heatInteractions.GetWreckingChange(copVehicle);
			manager->AddToPendingHeatChange(heatChange);
		}


		[[nodiscard]] static float __fastcall YieldPendingHeatChange(const address pursuit)
		{
			if (Globals::IsPursuitInCooldownMode(pursuit)) return 0.f;

			auto* const manager = HeatManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return 0.f);

			const float heatChange = manager->pendingHeatChange;

			if constexpr (Globals::loggingEnabled)
			{
				if (heatChange != 0.f)
					Globals::LogFull(pursuit, logTag, "Heat change:", heatChange);
			}

			manager->pendingHeatChange = 0.f;

			return Globals::floatScale * heatChange;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall ClampHeatLimits(const address pursuit)
	{
		float& minHeat = AsReference<float>(pursuit + 0x9C);
		float& maxHeat = AsReference<float>(pursuit + 0xA0);

		minHeat = HeatParameters::ClampHeat(minHeat);
		maxHeat = HeatParameters::ClampHeat(maxHeat);

		minHeat = std::min<float>(minHeat, maxHeat);
	}



	void __fastcall UpdateHeatAnimation(const address heatMeter)
	{
		const float  totalGameTime    = Globals::GetTotalGameTime();
		const size_t currentHeatLevel = static_cast<size_t>(AsReference<float>(heatMeter + 0x40));
		const bool   isNewHeatLevel   = (currentHeatLevel != lastAnimatedHeatLevel);

		lastAnimatedHeatLevel = currentHeatLevel; // update regardless of actual animation

		if (totalGameTime < animationEndTimestamp) return; // animation still active

		const auto IsFEngScriptSet = AsFunction<bool __cdecl (address, uint32_t)>      (0x514DA0);
		const auto SetFEngScript   = AsFunction<void __cdecl (address, uint32_t, bool)>(0x514D10);

		const address  interfaceObject = AsReference<address>(heatMeter + 0x44);
		const uint32_t animationScript = (isNewHeatLevel) ? 0x41E1FEDC : 0x1744B3;

		if (IsFEngScriptSet(interfaceObject, animationScript)) return; // script already set

		SetFEngScript(interfaceObject, animationScript, /* enabled = */ true);

		if (isNewHeatLevel)
			animationEndTimestamp = totalGameTime + 2.5f; // animation length (seconds)
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Ensures the Heat limits of newly created pursuits are valid
	ASSEMBLY_DETOUR(HeatLimits, /* begin = */ 0x443171, /* end = */ 0x44317A)
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x48]
			call ClampHeatLimits // ecx: pursuit

			// Execute original code and resume
			push ebx
			mov ecx, esi
			mov dword ptr [esi + 0xE0], ebx
			
			EXIT_ASSEMBLY_DETOUR(HeatLimits)
		}
	}



	// Adds pending Heat changes to racer-Heat updates
	ASSEMBLY_DETOUR(PassiveHeat, 0x443D4A, 0x443D50)
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
			EXIT_ASSEMBLY_DETOUR(PassiveHeat)
		}
	}



	// Increments the "spikes deployed" counter correctly
	ASSEMBLY_DETOUR(SpikeCounter, 0x43E654, 0x43E663)
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x10]
			cmp dword ptr [ecx], 3 // prop ID
			jne conclusion         // prop not spike strip

			mov edx, dword ptr [esp + 0x4C4] // roadblock pursuit
			inc dword ptr [edx + 0x17C]      // spike strips deployed

			conclusion:
			EXIT_ASSEMBLY_DETOUR(SpikeCounter)
		}
	}



	// Checks for support cops to increment deployment counter
	ASSEMBLY_DETOUR(SupportCheck, 0x423FA2, 0x423FF1)
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

			EXIT_ASSEMBLY_DETOUR(SupportCheck)
		}
	}



	// Notifies HeatManager of incurred property damage
	ASSEMBLY_DETOUR(PropertyDamage, 0x40945A, 0x409461)
	{
		__asm
		{
			push edx

			mov ecx, esi
			mov edx, dword ptr [esp + 0x14]
			call HeatManager::ProcessPropertyDamage // ecx: pursuit; edx: damageAmount

			pop edx

			// Execute original code and resume
			cmp dword ptr [edx + 0x1964], 2

			EXIT_ASSEMBLY_DETOUR(PropertyDamage)
		}
	}



	// Adjusts the Heat-escalation scale in Challenge Series events
	ASSEMBLY_DETOUR(ChallengeScale, 0x443D7B, 0x443D84)
	{
		static constexpr address IsChallengeEvent = 0x404AC0;
		static constexpr address challengeExit    = 0x443DAD;
		
		__asm
		{
			call dword ptr [IsChallengeEvent]
			test al, al
			jne challenge // is challenge event

			EXIT_ASSEMBLY_DETOUR(ChallengeScale)

			challenge:
			mov eax, offset challengeScale.current

			jmp dword ptr [challengeExit]
		}
	}



	// Prepares the meter state when a new Heat meter is created
	ASSEMBLY_DETOUR(HeatMeterReset, 0x59CEDF, 0x59CEE5)
	{
		__asm
		{
			// Execute original code first
			push edi
			xor ebx, ebx
			lea edi, dword ptr [esi + 0x8]

			mov dword ptr [lastAnimatedHeatLevel], ebx
			mov dword ptr [animationEndTimestamp], ebx

			EXIT_ASSEMBLY_DETOUR(HeatMeterReset)
		}
	}



	// Manages the Heat-transition animation of the Heat meter
	ASSEMBLY_DETOUR(HeatMeterUpdate, 0x56676D, 0x5667C8)
	{
		__asm
		{
			mov ecx, esi
			call UpdateHeatAnimation // ecx: heatMeter

			EXIT_ASSEMBLY_DETOUR(HeatMeterUpdate)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	void ExtractDamageChanges(const ConfigParser::Parser& parser)
	{
		HEAT_PARAMETER_VALUE(int, propertyToHeat, 0);

		HeatParameters::Extract(parser, "Heat:Property", propertyToHeat);

		for (const bool forRaces : {false, true})
		{
			const auto& damageArray = propertyToHeat    .GetHeatLevelArray(forRaces);
			auto&       changeArray = propertyHeatChange.GetHeatLevelArray(forRaces);

			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
				if (damageArray[heatLevelID] != 0) changeArray[heatLevelID] = 1.f / static_cast<float>(damageArray[heatLevelID]);
		}
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::fileHeat);

		// Heat parameters
		HeatParameters::Extract(parser, "Heat:Time", heatTimerEnabled);

		HeatParameters::Extract(parser, "Time:Challenges", challengeScale);

		HeatParameters::Extract(parser, "Heat:Deployments", chaserHeatChange, supportHeatChange, helicopterHeatChange);

		HeatParameters::Extract(parser, "Heat:Roadblocks", roadblockHeatChange, spikesHeatChange);

		HeatParameters::Extract(parser, "Heat:Traffic", trafficHitHeatChange);

		ExtractDamageChanges(parser);

		// Parameter sets
		heatInteractions.Extract(parser, "Heat");

		// Code modifications (general)
		MemoryTools::Write<byte>(0xEB, {0x44307F}); // Heat limits in Challenge Series events

		PATCH_ASSEMBLY_DETOUR(HeatLimits);
		PATCH_ASSEMBLY_DETOUR(PassiveHeat);
		PATCH_ASSEMBLY_DETOUR(SpikeCounter);
		PATCH_ASSEMBLY_DETOUR(SupportCheck);
		PATCH_ASSEMBLY_DETOUR(PropertyDamage);
		PATCH_ASSEMBLY_DETOUR(ChallengeScale);
		PATCH_ASSEMBLY_DETOUR(HeatMeterReset);
		PATCH_ASSEMBLY_DETOUR(HeatMeterUpdate);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		heatTimerEnabled.SetToHeatState(state);

		challengeScale.SetToHeatState(state);

		chaserHeatChange    .SetToHeatState(state);
		supportHeatChange   .SetToHeatState(state);
		helicopterHeatChange.SetToHeatState(state);

		roadblockHeatChange.SetToHeatState(state);
		spikesHeatChange   .SetToHeatState(state);

		heatInteractions.SetToHeatState(state);

		trafficHitHeatChange.SetToHeatState(state);

		propertyHeatChange.SetToHeatState(state);
	}



	void NotifyOfTaggedCop
	(
		const address copVehicle, 
		const address perpVehicle
	) {
		if (not anyFeatureEnabled) return;

		HeatManager::ProcessTaggedCop(copVehicle, perpVehicle);
	}



	void NotifyOfAssaultedCop
	(
		const address copVehicle,
		const address perpVehicle,
		const byte    numCopAssaulted
	) {
		if (not anyFeatureEnabled) return;

		HeatManager::ProcessAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
	}



	void NotifyOfDestroyedCop
	(
		const address pursuit, 
		const address copVehicle
	) {
		if (not anyFeatureEnabled) return;

		HeatManager::ProcessDestroyedCop(pursuit, copVehicle);
	}
}