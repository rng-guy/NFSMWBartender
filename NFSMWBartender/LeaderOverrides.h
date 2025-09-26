#pragma once

#include "Globals.h"
#include "HashContainers.h"
#include "PursuitFeatures.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace LeaderOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool>  leaderAggroEnableds (false);
	HeatParameters::Pair<float> minLeaderAggroDelays(1.f);
	HeatParameters::Pair<float> maxLeaderAggroDelays(1.f);

	HeatParameters::Pair<bool>  henchmenAggroEnableds (false);
	HeatParameters::Pair<float> minHenchmenAggroDelays(1.f);
	HeatParameters::Pair<float> maxHenchmenAggroDelays(1.f);

	HeatParameters::Pair<bool>  lostResetEnableds (false);
	HeatParameters::Pair<float> minLostResetDelays(1.f);
	HeatParameters::Pair<float> maxLostResetDelays(1.f);

	HeatParameters::Pair<bool>  wreckResetEnableds (false);
	HeatParameters::Pair<float> minWreckResetDelays(1.f);
	HeatParameters::Pair<float> maxWreckResetDelays(1.f);





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::PursuitReaction
	{
		using IntervalTimer = PursuitFeatures::IntervalTimer;



	private:

		int&  leaderFlag   = *((int*)(this->pursuit + 0x164));
		bool& skipPriority = *((bool*)(this->pursuit + 0x214));

		address leaderVehicle  = 0x0;
		bool    leaderIsAggro  = false;
		bool    hasNewHenchmen = false;

		HashContainers::AddressSet passiveHenchmenVehicles;

		IntervalTimer lostResetTimer     = IntervalTimer(lostResetEnableds,     minLostResetDelays,     maxLostResetDelays);
		IntervalTimer wreckResetTimer    = IntervalTimer(wreckResetEnableds,    minWreckResetDelays,    maxWreckResetDelays);
		IntervalTimer leaderAggroTimer   = IntervalTimer(leaderAggroEnableds,   minLeaderAggroDelays,   maxLeaderAggroDelays);
		IntervalTimer henchmenAggroTimer = IntervalTimer(henchmenAggroEnableds, minHenchmenAggroDelays, maxHenchmenAggroDelays);

		IntervalTimer* flagResetTimer = nullptr;


		bool LeaderCanSpawn() const
		{
			return (this->flagResetTimer) ? this->flagResetTimer->HasExpired() : true;
		}


		void UpdateLeaderFlag() const
		{
			this->leaderFlag = (this->leaderVehicle) ? 1 : ((this->LeaderCanSpawn()) ? 0 : 2);
		}


		void UpdateFlagResetTimer(const IntervalTimer::Setting setting)
		{
			if (not this->flagResetTimer) return;

			this->flagResetTimer->Update(setting);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->flagResetTimer->IsEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Flag reset in", this->flagResetTimer->TimeLeft());
			}
		}


		void UpdateLeaderAggroTimer(const IntervalTimer::Setting setting)
		{
			if ((not this->leaderVehicle) or (this->leaderIsAggro)) return;

			this->leaderAggroTimer.Update(setting);
	
			if constexpr (Globals::loggingEnabled)
			{
				if (this->leaderAggroTimer.IsEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Leader aggro in", this->leaderAggroTimer.TimeLeft());
			}
		}


		void UpdateHenchmenAggroTimer(const IntervalTimer::Setting setting)
		{
			if (this->passiveHenchmenVehicles.empty()) return;

			this->henchmenAggroTimer.Update(setting);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->henchmenAggroTimer.IsEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Henchmen aggro in", this->henchmenAggroTimer.TimeLeft());
			}
		}


		static bool MakeVehicleAggro(const address copVehicle)
		{
			static const auto SetSupportGoal = (void(__thiscall*)(address, vault))0x409850;
			const address     copAIVehicle   = *((address*)(copVehicle + 0x54));

			if (copAIVehicle)
				SetSupportGoal(copAIVehicle + 0x70C, 0x0);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [LDR] Invalid AIVehicle pointer for", copVehicle);

			return copAIVehicle;
		}


		void MakeHenchmenAggro()
		{
			for (const address henchmenVehicle : this->passiveHenchmenVehicles)
				this->MakeVehicleAggro(henchmenVehicle);

			if constexpr (Globals::loggingEnabled)
			{
				if (not this->passiveHenchmenVehicles.empty())
					Globals::logger.Log(this->pursuit, "[LDR] Henchmen now aggressive");
			}

			this->passiveHenchmenVehicles.clear();
		}


		void CheckAggroTimers()
		{
			if (this->leaderVehicle and (not this->leaderIsAggro) and this->leaderAggroTimer.HasExpired())
			{
				this->MakeVehicleAggro(this->leaderVehicle);
				this->leaderIsAggro = true;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Leader now aggressive");
			}
			
			if ((not this->passiveHenchmenVehicles.empty()) and this->henchmenAggroTimer.HasExpired())
				this->MakeHenchmenAggro();
		}



	public:

		explicit LeaderManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) {}


		void UpdateOnGameplay() override
		{
			this->CheckAggroTimers();
			this->UpdateLeaderFlag();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateFlagResetTimer    (IntervalTimer::Setting::LENGTH);
			this->UpdateLeaderAggroTimer  (IntervalTimer::Setting::LENGTH);
			this->UpdateHenchmenAggroTimer(IntervalTimer::Setting::LENGTH);

			this->UpdateLeaderFlag();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (copLabel != CopLabel::LEADER) return;

			if (not this->leaderVehicle)
			{
				this->leaderVehicle  = copVehicle;
				this->skipPriority   = true;
				this->leaderIsAggro  = false;
				this->hasNewHenchmen = false;
				this->flagResetTimer = nullptr;

				this->UpdateLeaderAggroTimer(IntervalTimer::Setting::ALL);
				this->UpdateLeaderFlag();
			}
			else
			{
				this->passiveHenchmenVehicles.insert(copVehicle);

				if (not this->hasNewHenchmen)
				{
					this->UpdateHenchmenAggroTimer(IntervalTimer::Setting::ALL);
					this->hasNewHenchmen = true;
				}
			}
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (copLabel != CopLabel::LEADER) return;

			if (this->leaderVehicle == copVehicle)
			{
				const bool isWrecked = PursuitFeatures::IsVehicleDestroyed(copVehicle);
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Leader", (isWrecked) ? "wrecked" : "lost");

				this->leaderVehicle  = 0x0;
				this->flagResetTimer = &((isWrecked) ? this->wreckResetTimer : this->lostResetTimer);

				this->UpdateFlagResetTimer(IntervalTimer::Setting::ALL);
				this->UpdateLeaderFlag();

				this->MakeHenchmenAggro();
			}
			else this->passiveHenchmenVehicles.erase(copVehicle);
		}
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini");

		// Heat parameters
		HeatParameters::ParseOptionalInterval
		(
			parser,
			"Leader:CrossAggro",
			leaderAggroEnableds,
			minLeaderAggroDelays,
			maxLeaderAggroDelays
		);

		HeatParameters::ParseOptionalInterval
		(
			parser,
			"Leader:HenchmenAggro",
			henchmenAggroEnableds,
			minHenchmenAggroDelays,
			maxHenchmenAggroDelays
		);

		HeatParameters::ParseOptionalInterval
		(
			parser, 
			"Leader:LostReset", 
			lostResetEnableds, 
			minLostResetDelays, 
			maxLostResetDelays
		);

		HeatParameters::ParseOptionalInterval
		(
			parser, 
			"Leader:WreckReset", 
			wreckResetEnableds, 
			minWreckResetDelays, 
			maxWreckResetDelays
		);

		// Code caves
		MemoryEditor::WriteToAddressRange(0x90, 0x42B6A2, 0x42B6B4); // Cross flag = 0
		MemoryEditor::WriteToAddressRange(0x90, 0x42402A, 0x424036); // Cross flag = 1
		MemoryEditor::WriteToAddressRange(0x90, 0x42B631, 0x42B643); // Cross flag = 2

		// Status flag
		featureEnabled = true;

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		leaderAggroEnableds .SetToHeat(isRacing, heatLevel);
		minLeaderAggroDelays.SetToHeat(isRacing, heatLevel);
		maxLeaderAggroDelays.SetToHeat(isRacing, heatLevel);

		henchmenAggroEnableds .SetToHeat(isRacing, heatLevel);
		minHenchmenAggroDelays.SetToHeat(isRacing, heatLevel);
		maxHenchmenAggroDelays.SetToHeat(isRacing, heatLevel);

		lostResetEnableds .SetToHeat(isRacing, heatLevel);
		minLostResetDelays.SetToHeat(isRacing, heatLevel);
		maxLostResetDelays.SetToHeat(isRacing, heatLevel);

		wreckResetEnableds .SetToHeat(isRacing, heatLevel);
		minWreckResetDelays.SetToHeat(isRacing, heatLevel);
		maxWreckResetDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			const bool anyEnabled = 
			(
				leaderAggroEnableds.current 
				or henchmenAggroEnableds.current 
				or lostResetEnableds.current 
				or wreckResetEnableds.current
			);

			if (anyEnabled)
				Globals::logger.Log("    HEAT [LDR] LeaderOverrides");
			
			if (leaderAggroEnableds.current)
				Globals::logger.LogLongIndent("leaderAggroDelay        ", minLeaderAggroDelays.current, "to", maxLeaderAggroDelays.current);

			if (henchmenAggroEnableds.current)
				Globals::logger.LogLongIndent("henchmenAggroDelay      ", minHenchmenAggroDelays.current, "to", maxHenchmenAggroDelays.current);

			if (lostResetEnableds.current)
				Globals::logger.LogLongIndent("escapeResetDelay        ", minLostResetDelays.current, "to", maxLostResetDelays.current);

			if (wreckResetEnableds.current)
				Globals::logger.LogLongIndent("wreckResetDelay         ", minWreckResetDelays.current, "to", maxWreckResetDelays.current);
		}
	}
}