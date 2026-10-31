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

	// Feature flag shared with (and managed by) AdvancedFeatures header

	// Logging
	constexpr Globals::LogLiteral logTag  = "[HCH]";
	constexpr Globals::LogLiteral logName = "HeatChangeOverrides";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, heatTimerEnabled, true);

	constinit HEAT_PARAMETER_VALUE(float, challengeEscalation, 600.f, {0.f}); // unity

	constinit HEAT_PARAMETER_VALUE(float, chaserHeatChange,     0.f); // levels
	constinit HEAT_PARAMETER_VALUE(float, supportHeatChange,    0.f); // levels
	constinit HEAT_PARAMETER_VALUE(float, helicopterHeatChange, 0.f); // levels

	constinit HEAT_PARAMETER_VALUE(float, roadblockHeatChange, 0.f); // levels
	constinit HEAT_PARAMETER_VALUE(float, spikesHeatChange,    0.f); // levels

	constinit HEAT_PARAMETER_VALUE(float, trafficHitHeatChange, 0.f); // levels

	constinit HEAT_PARAMETER_VALUE(int, damageToHeatLevel, 0); // dollars

	// Cop interactions
	RELEASE_CONSTINIT ParameterSets::CopInteractions heatInteractions; // levels

	// Parameter conversions
	float damageHeatChange; // levels

	// Detour data
	size_t lastAnimatedHeatLevel = 0;
	float  animationEndTimestamp = 0.f; // seconds





	// CountTracker class ---------------------------------------------------------------------------------------------------------------------------

	class CountTracker
	{
	private: // aliases

		using ChangeValue = HeatParameters::Value<float>;



	private: // members

		int lastCount = 0;

		const int&         count;        // pursuit-locked (through count) and immobile
		const ChangeValue& heatPerCount; // count-locked upon construction


	public: // methods

		CountTracker
		(
			const address      pursuit,
			const ptrdiff_t    countOffset,
			const ChangeValue& heatPerCount
		)
			: count(AsReference<int>(pursuit + countOffset)), heatPerCount(heatPerCount)
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





	// HeatManager class ----------------------------------------------------------------------------------------------------------------------------

	class HeatManager : public PursuitFeatures::Reaction, public PursuitFeatures::Searchable<HeatManager>
	{
	private: // members

		float pendingHeatChange = 0.f; // levels

		std::array<CountTracker, 6> countTrackers =
		{
			CountTracker(this->pursuit, 0x184, chaserHeatChange),
			CountTracker(this->pursuit, 0x188, supportHeatChange),
			CountTracker(this->pursuit, 0x150, helicopterHeatChange),
			CountTracker(this->pursuit, 0x158, roadblockHeatChange),
			CountTracker(this->pursuit, 0x17C, spikesHeatChange),
			CountTracker(this->pursuit, 0x168, trafficHitHeatChange)
		};
	

	private: // methods

		void AddToPendingHeatChange(const float levels)
		{
			if (Globals::Pursuit::IsSearching(this->pursuit)) return;

			this->pendingHeatChange += levels;
		}


		void ProcessTrackerYields()
		{
			float totalHeatChange = 0.f; // levels

			for (CountTracker& tracker : this->countTrackers)
				totalHeatChange += tracker.YieldHeatChange();

			this->AddToPendingHeatChange(totalHeatChange);
		}


		[[nodiscard]] static HeatManager* FindInstanceByPerp(const address perpVehicle)
		{
			return HeatManager::FindInstance(Globals::PerpVehicle::GetPursuit(perpVehicle));
		}


	public: // methods

		explicit HeatManager(const address pursuit) : PursuitFeatures::Reaction(pursuit) {}


		void ReactToGameplay() override
		{
			this->ProcessTrackerYields();
		}


		static void ProcessTaggedCop
		(
			const address copVehicle, 
			const address perpVehicle
		) {
			auto* const manager = HeatManager::FindInstanceByPerp(perpVehicle);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->AddToPendingHeatChange(heatInteractions.GetTaggingChange(copVehicle));
		}


		static void ProcessAssaultedCop
		(
			const address copVehicle,
			const address perpVehicle,
			const byte    numCopAssaulted
		) {
			auto* const manager = HeatManager::FindInstanceByPerp(perpVehicle);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->AddToPendingHeatChange(heatInteractions.GetAssaultChange(copVehicle, numCopAssaulted));
		}


		static void __fastcall ProcessPropertyDamage
		(
			const address pursuit,
			const int     damage
		) {
			auto* const manager = HeatManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->AddToPendingHeatChange(static_cast<float>(damage) * damageHeatChange);
		}


		static void ProcessDestroyedCop
		(
			const address pursuit,
			const address copVehicle
		) {
			auto* const manager = HeatManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->AddToPendingHeatChange(heatInteractions.GetWreckingChange(copVehicle));
		}


		[[nodiscard]] static float __fastcall YieldPendingHeatChange(const address pursuit)
		{
			if (Globals::Pursuit::IsSearching(pursuit)) return 0.f;

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

	[[nodiscard]] bool __cdecl IsCareerEvent()
	{
		return (Globals::raceStatus and (AsReference<int>(Globals::raceStatus + 0x1964) == 2)); // race context
	}



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
		const float timestamp = Globals::GetTotalGameTime();

		const size_t currentHeatLevel = static_cast<size_t>(AsReference<float>(heatMeter + 0x40));
		const bool   isNewHeatLevel   = (currentHeatLevel != lastAnimatedHeatLevel);

		lastAnimatedHeatLevel = currentHeatLevel; // update regardless of animation

		if (timestamp < animationEndTimestamp) return; // animation still active

		const auto IsFEngScriptSet = AsFunction<bool __cdecl (address, uint32_t)>      (0x514DA0);
		const auto SetFEngScript   = AsFunction<void __cdecl (address, uint32_t, bool)>(0x514D10);

		const address  heatLevelIcon   = AsReference<address>(heatMeter + 0x44);
		const uint32_t animationScript = (isNewHeatLevel) ? 0x41E1FEDC : 0x1744B3;

		if (IsFEngScriptSet(heatLevelIcon, animationScript)) return; // script already set

		SetFEngScript(heatLevelIcon, animationScript, /* enabled = */ true);

		if (isNewHeatLevel)
			animationEndTimestamp = timestamp + 2.5f; // animation length
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



	// Checks whether the current event is the "epic" pursuit
	ASSEMBLY_DETOUR(EpicPursuit, 0x443004, 0x443023)
	{
		static constexpr address IsEpicPursuit = 0x5FC560;
		static constexpr address epicExit      = 0x44306B;

		__asm
		{
			push eax
			push ecx

			call dword ptr [IsEpicPursuit]
			test al, al
			jne epic // is "epic" pursuit
			
			add esp, 0x8

			EXIT_ASSEMBLY_DETOUR(EpicPursuit)

			epic:
			pop ecx
			pop eax

			jmp dword ptr [epicExit]
		}
	}



	// Updates the pursuit target's Heat value
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



	// Increments the "spikes deployed" counter
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



	// Processes incoming generic property damage
	ASSEMBLY_DETOUR(PropertyDamage, 0x40945A, 0x409461)
	{
		__asm
		{
			push edx

			mov ecx, esi
			mov edx, dword ptr [esp + 0x14]
			call HeatManager::ProcessPropertyDamage // ecx: pursuit; edx: damage

			pop edx

			// Execute original code and resume
			cmp dword ptr [edx + 0x1964], 2

			EXIT_ASSEMBLY_DETOUR(PropertyDamage)
		}
	}



	// Fetches the current Heat-escalation scale
	ASSEMBLY_DETOUR(ChallengeScale, 0x443D7B, 0x443D84)
	{
		static constexpr address challengeExit = 0x443DAD;
		
		__asm
		{
			call IsCareerEvent
			test al, al
			je challenge // is challenge event

			EXIT_ASSEMBLY_DETOUR(ChallengeScale)

			challenge:
			mov eax, offset challengeEscalation.current

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

	void UpdateParameterConversions()
	{
		damageHeatChange = (damageToHeatLevel.current != 0) ? (1.f / static_cast<float>(damageToHeatLevel.current)) : 0.f;
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::Files::heat);

		// Heat parameters
		HeatParameters::Extract(parser, "Heat:Time", heatTimerEnabled);

		HeatParameters::Extract(parser, "Time:Challenges", challengeEscalation);

		HeatParameters::Extract(parser, "Heat:Deployments", chaserHeatChange, supportHeatChange, helicopterHeatChange);

		HeatParameters::Extract(parser, "Heat:Roadblocks", roadblockHeatChange, spikesHeatChange);

		HeatParameters::Extract(parser, "Heat:Traffic", trafficHitHeatChange);

		HeatParameters::Extract(parser, "Heat:Property", damageToHeatLevel);

		// Cop interactions
		heatInteractions.Extract(parser, "Heat");

		// Parameter conversions
		UpdateParameterConversions(); // uses vanilla value(s)

		// Code modifications
		MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x443062, 0x4430D6}); // global Heat limit

		MemoryTools::MakeRangeNOP<0x44306B, 0x44309F>(); // Heat limits in Challenge Series events

		PATCH_ASSEMBLY_DETOUR(HeatLimits);
		PATCH_ASSEMBLY_DETOUR(EpicPursuit);
		PATCH_ASSEMBLY_DETOUR(PassiveHeat);
		PATCH_ASSEMBLY_DETOUR(SpikeCounter);
		PATCH_ASSEMBLY_DETOUR(SupportCheck);
		PATCH_ASSEMBLY_DETOUR(PropertyDamage);
		PATCH_ASSEMBLY_DETOUR(ChallengeScale);
		PATCH_ASSEMBLY_DETOUR(HeatMeterReset);
		PATCH_ASSEMBLY_DETOUR(HeatMeterUpdate);

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		// Heat parameters
		heatTimerEnabled.SetToHeatState(state);

		challengeEscalation.SetToHeatState(state);

		chaserHeatChange    .SetToHeatState(state);
		supportHeatChange   .SetToHeatState(state);
		helicopterHeatChange.SetToHeatState(state);

		roadblockHeatChange.SetToHeatState(state);
		spikesHeatChange   .SetToHeatState(state);

		trafficHitHeatChange.SetToHeatState(state);

		damageToHeatLevel.SetToHeatState(state);

		// Cop interactions
		heatInteractions.SetToHeatState(state);

		// Parameter conversions
		UpdateParameterConversions();
	}



	void NotifyOfTaggedCop
	(
		const address copVehicle, 
		const address perpVehicle
	) {
		HeatManager::ProcessTaggedCop(copVehicle, perpVehicle);
	}



	void NotifyOfAssaultedCop
	(
		const address copVehicle,
		const address perpVehicle,
		const byte    numCopAssaulted
	) {
		HeatManager::ProcessAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
	}



	void NotifyOfDestroyedCop
	(
		const address pursuit, 
		const address copVehicle
	) {
		HeatManager::ProcessDestroyedCop(pursuit, copVehicle);
	}
}