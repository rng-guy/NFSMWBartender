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

	HeatParameters::Pair<bool>  lossResetEnableds (false);
	HeatParameters::Pair<float> minLossResetDelays(1.f);
	HeatParameters::Pair<float> maxLossResetDelays(1.f);

	HeatParameters::Pair<bool>  wreckResetEnableds (false);
	HeatParameters::Pair<float> minWreckResetDelays(1.f);
	HeatParameters::Pair<float> maxWreckResetDelays(1.f);





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::PursuitReaction
	{
		using IntervalTimer = PursuitFeatures::IntervalTimer;



	private:

		int* const leaderFlag = (int*)(this->pursuit + 0x164);

		address leaderVehicle = 0x0;
		bool    leaderIsAggro = false;

		HashContainers::AddressSet passiveHenchmenVehicles;

		IntervalTimer lossResetTimer     = IntervalTimer(lossResetEnableds,     minLossResetDelays,     maxLossResetDelays);
		IntervalTimer wreckResetTimer    = IntervalTimer(wreckResetEnableds,    minWreckResetDelays,    maxWreckResetDelays);
		IntervalTimer leaderAggroTimer   = IntervalTimer(leaderAggroEnableds,   minLeaderAggroDelays,   maxLeaderAggroDelays);
		IntervalTimer henchmenAggroTimer = IntervalTimer(henchmenAggroEnableds, minHenchmenAggroDelays, maxHenchmenAggroDelays);

		IntervalTimer* resetTimer = nullptr;


		bool LeaderCanSpawn() const
		{
			return (this->resetTimer) ? this->resetTimer->HasExpired() : true;
		}


		void UpdateLeaderFlag() const
		{
			*(this->leaderFlag) = (this->leaderVehicle) ? 1 : ((this->LeaderCanSpawn()) ? 0 : 2);
		}


		void UpdateResetTimer(const IntervalTimer::Setting setting)
		{
			if (not this->resetTimer) return;

			this->resetTimer->Update(setting);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->resetTimer->IsEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Flag reset in", this->resetTimer->TimeLeft());
			}
		}


		void MakeVehicleAggressive(const address copVehicle) const
		{
			static void (__thiscall* const SetSupportGoal)(address, vault) = (void (__thiscall*)(address, vault))0x409850;
			
			const address copAIVehicle = *((address*)(copVehicle + 0x54));

			if (copAIVehicle)
				SetSupportGoal(copAIVehicle + 0x70C, 0x0);
		}


		void UpdateAggroTimers(const IntervalTimer::Setting setting)
		{
			if (this->leaderVehicle and (not leaderIsAggro))
			{
				this->leaderAggroTimer.Update(setting);
	
				if constexpr (Globals::loggingEnabled)
				{
					if (this->leaderAggroTimer.IsEnabled())
						Globals::logger.Log(this->pursuit, "[LDR] Leader aggro in", this->leaderAggroTimer.TimeLeft());
				}
			}

			if (this->leaderVehicle or (not this->passiveHenchmenVehicles.empty()))
			{
				this->henchmenAggroTimer.Update(setting);

				if constexpr (Globals::loggingEnabled)
				{
					if (this->henchmenAggroTimer.IsEnabled())
						Globals::logger.Log(this->pursuit, "[LDR] Henchmen aggro in", this->henchmenAggroTimer.TimeLeft());
				}
			}
		}


		void MakeHenchmenAggressive()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[LDR] Henchmen now aggressive");

			for (const address henchmenVehicle : this->passiveHenchmenVehicles)
				this->MakeVehicleAggressive(henchmenVehicle);

			this->passiveHenchmenVehicles.clear();
		}


		void CheckAggroTimers()
		{
			if (this->leaderVehicle and (not leaderIsAggro))
			{
				if (this->leaderAggroTimer.HasExpired())
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[LDR] Leader now aggressive");

					this->MakeVehicleAggressive(this->leaderVehicle);
					this->leaderIsAggro = true;
				}
			}
			
			if (not this->passiveHenchmenVehicles.empty())
			{
				if (this->henchmenAggroTimer.HasExpired())
					this->MakeHenchmenAggressive();
			}
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
			this->UpdateResetTimer(IntervalTimer::Setting::LENGTH);
			this->UpdateAggroTimers(IntervalTimer::Setting::LENGTH);
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
				this->leaderVehicle = copVehicle;
				this->leaderIsAggro = false;
				this->resetTimer    = nullptr;

				this->UpdateAggroTimers(IntervalTimer::Setting::ALL);
				this->UpdateLeaderFlag();
			}
			else this->passiveHenchmenVehicles.insert(copVehicle);
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

				this->leaderVehicle = 0x0;
				this->resetTimer    = &((isWrecked) ? this->wreckResetTimer : this->lossResetTimer);

				if (not this->passiveHenchmenVehicles.empty())
					this->MakeHenchmenAggressive();

				this->UpdateResetTimer(IntervalTimer::Setting::ALL);
				this->UpdateLeaderFlag();
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
			"Leader:LossReset", 
			lossResetEnableds, 
			minLossResetDelays, 
			maxLossResetDelays
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
		MemoryEditor::WriteToAddressRange(0x90, 0x42B6AA, 0x42B6B4); // Cross flag to 0
		MemoryEditor::WriteToAddressRange(0x90, 0x42402C, 0x424036); // Cross flag to 1
		MemoryEditor::WriteToAddressRange(0x90, 0x42B631, 0x42B643); // Cross flag to 2

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

		lossResetEnableds .SetToHeat(isRacing, heatLevel);
		minLossResetDelays.SetToHeat(isRacing, heatLevel);
		maxLossResetDelays.SetToHeat(isRacing, heatLevel);

		wreckResetEnableds .SetToHeat(isRacing, heatLevel);
		minWreckResetDelays.SetToHeat(isRacing, heatLevel);
		maxWreckResetDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			const bool anyEnabled = 
			(
				   leaderAggroEnableds.current 
				or henchmenAggroEnableds.current 
				or lossResetEnableds.current 
				or wreckResetEnableds.current
			);

			if (anyEnabled)
				Globals::logger.Log("    HEAT [LDR] LeaderOverrides");
			
			if (leaderAggroEnableds.current)
				Globals::logger.LogLongIndent("leaderAggroDelay        ", minLeaderAggroDelays.current, "to", maxLeaderAggroDelays.current);

			if (henchmenAggroEnableds.current)
				Globals::logger.LogLongIndent("henchmenAggroDelay      ", minHenchmenAggroDelays.current, "to", maxHenchmenAggroDelays.current);

			if (lossResetEnableds.current)
				Globals::logger.LogLongIndent("escapeResetDelay        ", minLossResetDelays.current, "to", maxLossResetDelays.current);

			if (wreckResetEnableds.current)
				Globals::logger.LogLongIndent("wreckResetDelay         ", minWreckResetDelays.current, "to", maxWreckResetDelays.current);
		}
	}
}