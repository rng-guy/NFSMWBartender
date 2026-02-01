#pragma once

#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace LeaderOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	HeatParameters::OptionalInterval<float> leader5CrossAggroDelays ({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader5ExpireResetDelays({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader5WreckResetDelays ({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader5LostResetDelays  ({1.f}); // seconds

	HeatParameters::OptionalInterval<float> leader7CrossAggroDelays ({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader7HenchAggroDelays ({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader7ExpireResetDelays({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader7WreckResetDelays ({1.f}); // seconds
	HeatParameters::OptionalInterval<float> leader7LostResetDelays  ({1.f}); // seconds





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::PursuitReaction
	{
	private:

		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST
		};

		int lastStrategyID = 5;

		address crossVehicle = 0x0;
		Status  crossStatus  = Status::PENDING;

		float expirationTimestamp = Globals::simulationTime;

		volatile int&  crossFlag    = *reinterpret_cast<volatile int*> (this->pursuit + 0x164);
		volatile bool& skipPriority = *reinterpret_cast<volatile bool*>(this->pursuit + 0x214);

		const volatile address& leaderStrategy = *reinterpret_cast<volatile address*>(this->pursuit + 0x198);

		HashContainers::AddressSet passiveHenchmenVehicles;

		PursuitFeatures::IntervalTimer flagResetTimer;
		PursuitFeatures::IntervalTimer crossAggroTimer;
		PursuitFeatures::IntervalTimer henchmenAggroTimer;


		void SetCrossStatus(const Status status)
		{
			std::string statusName;

			switch (status)
			{
			case Status::PENDING:
				statusName      = "(available)";
				this->crossFlag = 0;
				break;

			case Status::ACTIVE:
				statusName      = "(active)";
				this->crossFlag = 1;
				break;

			default:
				statusName      = "(blocked)";
				this->crossFlag = 2;
			}

			this->crossStatus = status;

			// With logging disabled, the compiler optimises "statusName" away
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[LDR] Cross flag now", this->crossFlag, statusName);
		}


		void CheckFlagResetTimer()
		{
			if (not this->flagResetTimer.HasExpired()) return;

			this->flagResetTimer.Stop();
			this->SetCrossStatus(Status::PENDING);
		}


		void UpdateFlagResetTimer()
		{
			std::string statusName;

			const bool isLeader7 = (this->lastStrategyID == 7);

			switch (this->crossStatus)
			{
			case Status::EXPIRED:
				statusName = "(expire)";
				this->flagResetTimer.LoadInterval((isLeader7) ? leader7ExpireResetDelays : leader5ExpireResetDelays);
				break;

			case Status::WRECKED:
				statusName = "(wreck)";
				this->flagResetTimer.LoadInterval((isLeader7) ? leader7WreckResetDelays : leader5WreckResetDelays);
				break;

			case Status::LOST:
				statusName = "(lost)";
				this->flagResetTimer.LoadInterval((isLeader7) ? leader7LostResetDelays : leader5LostResetDelays);
				break;

			default:
				return; // e.g. Status::PENDING
			}

			if (not flagResetTimer.IsSet())
				this->flagResetTimer.Start();

			// With logging disabled, the compiler optimises "statusName" away
			if constexpr (Globals::loggingEnabled)
			{
				if (this->flagResetTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Flag reset", statusName, "in", this->flagResetTimer.GetTimeLeft());

				else
					Globals::logger.Log(this->pursuit, "[LDR] Flag reset", statusName, "suspended");
			}
		}


		void MakeHenchmenAggro()
		{
			this->henchmenAggroTimer.Stop();

			for (const address copVehicle : this->passiveHenchmenVehicles)
				this->EndSupportGoal(copVehicle);

			if constexpr (Globals::loggingEnabled)
			{
				if (not this->passiveHenchmenVehicles.empty())
					Globals::logger.Log(this->pursuit, "[LDR] Henchmen now aggressive");
			}

			this->passiveHenchmenVehicles.clear();
		}


		void CheckAggroTimers()
		{
			if (this->crossAggroTimer.HasExpired())
			{
				this->crossAggroTimer.Stop();
				this->EndSupportGoal(this->crossVehicle);
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Cross now aggressive");
			}
			
			if (this->henchmenAggroTimer.HasExpired())
				this->MakeHenchmenAggro();
		}


		void UpdateExpirationTimestamp()
		{
			this->expirationTimestamp = Globals::simulationTime;

			if (this->leaderStrategy)
				this->expirationTimestamp += *reinterpret_cast<volatile float*>(this->leaderStrategy + 0x8);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [LDR] Invalid duration pointer in", this->pursuit);
		}



	public:

		explicit LeaderManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "LeaderManager");
		}


		~LeaderManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "LeaderManager");
		}


		void UpdateOnGameplay() override
		{
			this->CheckAggroTimers();
			this->CheckFlagResetTimer();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateFlagResetTimer();
			this->CheckFlagResetTimer();
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
				this->flagResetTimer.Stop();

				this->skipPriority = true;

				this->UpdateExpirationTimestamp();
				this->SetCrossStatus(Status::ACTIVE);

				this->lastStrategyID = (this->leaderStrategy) ? *reinterpret_cast<volatile int*>(this->leaderStrategy) : 5;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Strategy ID now", this->lastStrategyID);

				this->crossAggroTimer.LoadInterval((this->lastStrategyID == 7) ? leader7CrossAggroDelays : leader5CrossAggroDelays);
				this->crossAggroTimer.Start();

				if constexpr (Globals::loggingEnabled)
				{
					if (this->crossAggroTimer.IsIntervalEnabled())
						Globals::logger.Log(this->pursuit, "[LDR] Cross aggro in", this->crossAggroTimer.GetLength());
				}
			}
			else
			{
				this->passiveHenchmenVehicles.insert(copVehicle);

				if (not this->henchmenAggroTimer.IsSet())
				{
					this->henchmenAggroTimer.LoadInterval(leader7HenchAggroDelays);
					this->henchmenAggroTimer.Start();

					if constexpr (Globals::loggingEnabled)
					{
						if (this->henchmenAggroTimer.IsIntervalEnabled())
							Globals::logger.Log(this->pursuit, "[LDR] Henchmen aggro in", this->henchmenAggroTimer.GetLength());
					}
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
				this->crossVehicle = 0x0;
				this->crossAggroTimer.Stop();

				if (Globals::IsVehicleDestroyed(copVehicle))
					this->SetCrossStatus(Status::WRECKED);
				
				else if (Globals::simulationTime >= this->expirationTimestamp)
					this->SetCrossStatus(Status::EXPIRED);
				
				else				
					this->SetCrossStatus(Status::LOST);
				
				this->UpdateFlagResetTimer();
				this->MakeHenchmenAggro();
			}
			else if (this->passiveHenchmenVehicles.erase(copVehicle))
			{
				if (this->passiveHenchmenVehicles.empty())
					this->henchmenAggroTimer.Stop();
			}
		}
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [LDR] LeaderOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Strategies.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Leader5:CrossAggro",  leader5CrossAggroDelays);
		HeatParameters::Parse(parser, "Leader5:ExpireReset", leader5ExpireResetDelays);
		HeatParameters::Parse(parser, "Leader5:WreckReset",  leader5WreckResetDelays);
		HeatParameters::Parse(parser, "Leader5:LostReset",   leader5LostResetDelays);

		HeatParameters::Parse(parser, "Leader7:CrossAggro",    leader7CrossAggroDelays);
		HeatParameters::Parse(parser, "Leader7:HenchmenAggro", leader7HenchAggroDelays);
		HeatParameters::Parse(parser, "Leader7:ExpireReset",   leader7ExpireResetDelays);
		HeatParameters::Parse(parser, "Leader7:WreckReset",    leader7WreckResetDelays);
		HeatParameters::Parse(parser, "Leader7:LostReset",     leader7LostResetDelays);

		// Code modifications
		MemoryTools::MakeRangeNOP(0x42B6A2, 0x42B6B4); // Cross flag = 0
		MemoryTools::MakeRangeNOP(0x42402A, 0x424036); //              1
		MemoryTools::MakeRangeNOP(0x42B631, 0x42B643); //              2

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		if (
			leader5CrossAggroDelays.isEnableds.current
			or leader5ExpireResetDelays.isEnableds.current
			or leader5WreckResetDelays.isEnableds.current
			or leader5LostResetDelays.isEnableds.current
			or leader7CrossAggroDelays.isEnableds.current
			or leader7HenchAggroDelays.isEnableds.current
			or leader7ExpireResetDelays.isEnableds.current
			or leader7WreckResetDelays.isEnableds.current
			or leader7LostResetDelays.isEnableds.current
		   )
		{
			Globals::logger.Log("    HEAT [LDR] LeaderOverrides");

			leader5CrossAggroDelays .Log("leader5CrossAggroDelay  ");
			leader5ExpireResetDelays.Log("leader5ExpireResetDelay ");
			leader5WreckResetDelays .Log("leader5WreckResetDelays ");
			leader5LostResetDelays  .Log("leader5LostResetDelays  ");

			leader7CrossAggroDelays .Log("leader7CrossAggroDelay  ");
			leader7HenchAggroDelays .Log("leader7HenchAggroDelay  ");
			leader7ExpireResetDelays.Log("leader7ExpireResetDelay ");
			leader7WreckResetDelays .Log("leader7WreckResetDelays ");
			leader7LostResetDelays  .Log("leader7LostResetDelays  ");
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		leader5CrossAggroDelays .SetToHeat(isRacing, heatLevel);
		leader5ExpireResetDelays.SetToHeat(isRacing, heatLevel);
		leader5WreckResetDelays .SetToHeat(isRacing, heatLevel);
		leader5LostResetDelays  .SetToHeat(isRacing, heatLevel);

		leader7CrossAggroDelays .SetToHeat(isRacing, heatLevel);
		leader7HenchAggroDelays .SetToHeat(isRacing, heatLevel);
		leader7ExpireResetDelays.SetToHeat(isRacing, heatLevel);
		leader7WreckResetDelays .SetToHeat(isRacing, heatLevel);
		leader7LostResetDelays  .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}