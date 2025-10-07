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
	HeatParameters::OptionalInterval crossAggroDelays;
	HeatParameters::OptionalInterval henchmenAggroDelays;
	HeatParameters::OptionalInterval lostResetDelays;
	HeatParameters::OptionalInterval wreckResetDelays;





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::PursuitReaction
	{
		using IntervalTimer = PursuitFeatures::IntervalTimer;



	private:

		int&  crossFlag   = *((int*)(this->pursuit + 0x164));
		bool& skipPriority = *((bool*)(this->pursuit + 0x214));

		address crossVehicle   = 0x0;
		bool    crossIsAggro   = false;
		bool    hasNewHenchmen = false;

		HashContainers::AddressSet passiveHenchmenVehicles;

		IntervalTimer lostResetTimer     = IntervalTimer(lostResetDelays);
		IntervalTimer wreckResetTimer    = IntervalTimer(wreckResetDelays);
		IntervalTimer crossAggroTimer    = IntervalTimer(crossAggroDelays);
		IntervalTimer henchmenAggroTimer = IntervalTimer(henchmenAggroDelays);

		IntervalTimer* flagResetTimer = nullptr;


		bool LeaderCanSpawn() const
		{
			return (this->flagResetTimer) ? this->flagResetTimer->HasExpired() : true;
		}


		void UpdateCrossFlag() const
		{
			this->crossFlag = (this->crossVehicle) ? 1 : ((this->LeaderCanSpawn()) ? 0 : 2);
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


		void UpdateCrossAggroTimer(const IntervalTimer::Setting setting)
		{
			if ((not this->crossVehicle) or (this->crossIsAggro)) return;

			this->crossAggroTimer.Update(setting);
	
			if constexpr (Globals::loggingEnabled)
			{
				if (this->crossAggroTimer.IsEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Leader aggro in", this->crossAggroTimer.TimeLeft());
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
			if (this->crossVehicle and (not this->crossIsAggro) and this->crossAggroTimer.HasExpired())
			{
				this->MakeVehicleAggro(this->crossVehicle);
				this->crossIsAggro = true;

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
			this->UpdateCrossFlag();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateFlagResetTimer    (IntervalTimer::Setting::LENGTH);
			this->UpdateCrossAggroTimer   (IntervalTimer::Setting::LENGTH);
			this->UpdateHenchmenAggroTimer(IntervalTimer::Setting::LENGTH);

			this->UpdateCrossFlag();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (copLabel != CopLabel::LEADER) return;

			if (not this->crossVehicle) // Cross always joins first
			{
				this->crossVehicle = copVehicle;

				this->skipPriority   = true;
				this->crossIsAggro   = false;
				this->hasNewHenchmen = false;
				this->flagResetTimer = nullptr;

				this->UpdateCrossAggroTimer(IntervalTimer::Setting::ALL);
				this->UpdateCrossFlag();
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

			if (this->crossVehicle == copVehicle)
			{
				const bool isWrecked = this->IsVehicleDestroyed(copVehicle);
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Leader", (isWrecked) ? "wrecked" : "lost");

				this->crossVehicle   = 0x0;
				this->flagResetTimer = &((isWrecked) ? this->wreckResetTimer : this->lostResetTimer);

				this->UpdateFlagResetTimer(IntervalTimer::Setting::ALL);
				this->UpdateCrossFlag();

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
		HeatParameters::Parse(parser, "Leader:CrossAggro",    crossAggroDelays);
		HeatParameters::Parse(parser, "Leader:HenchmenAggro", henchmenAggroDelays);
		HeatParameters::Parse(parser, "Leader:LostReset",     lostResetDelays);
		HeatParameters::Parse(parser, "Leader:WreckReset",    wreckResetDelays);

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

		crossAggroDelays   .SetToHeat(isRacing, heatLevel);
		henchmenAggroDelays.SetToHeat(isRacing, heatLevel);
		lostResetDelays    .SetToHeat(isRacing, heatLevel);
		wreckResetDelays   .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			if (
				crossAggroDelays.isEnableds.current 
				or henchmenAggroDelays.isEnableds.current 
				or lostResetDelays.isEnableds.current 
				or wreckResetDelays.isEnableds.current
			   )
				Globals::logger.Log("    HEAT [LDR] LeaderOverrides");
			
			crossAggroDelays   .Log("crossAggroDelay         ");
			henchmenAggroDelays.Log("henchmenAggroDelay      ");
			lostResetDelays    .Log("lostResetDelay          ");
			wreckResetDelays   .Log("wreckResetDelay         ");
		}
	}
}