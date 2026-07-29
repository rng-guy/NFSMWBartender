#pragma once

#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace LeaderOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::OptionalInterval<float> leader5CrossAggroDelay ({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader5ExpireResetDelay({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader5WreckResetDelay ({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader5LostResetDelay  ({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader7CrossAggroDelay ({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader7HenchAggroDelay ({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader7ExpireResetDelay({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader7WreckResetDelay ({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> leader7LostResetDelay  ({1.f}); // seconds





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::PursuitReaction
	{
	private:

		// Internal enum
		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST
		};


	private:

		int lastStrategyID = 5;

		address crossVehicle = 0x0;
		Status  crossStatus  = Status::PENDING;

		float expirationTimestamp = Globals::simulationTime;

		int&  crossFlag    = AsReference<int> (this->pursuit + 0x164);
	    bool& skipPriority = AsReference<bool>(this->pursuit + 0x214);

		const address& leaderStrategy = AsReference<address>(this->pursuit + 0x198);

		ModContainers::AddressSet passiveHenchmenVehicles;

		PursuitFeatures::IntervalTimer flagResetTimer;
		PursuitFeatures::IntervalTimer crossAggroTimer;
		PursuitFeatures::IntervalTimer henchmenAggroTimer;


		void SetCrossStatus(const Status status)
		{
			std::string_view statusName;

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

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[LDR] Cross flag now", this->crossFlag, statusName);
		}


		void CheckFlagResetTimer()
		{
			if (not this->flagResetTimer.HasExpired()) return;

			this->flagResetTimer.Stop();
			this->SetCrossStatus(Status::PENDING);
		}


		void ProcessCrossExpiration()
		{
			switch (this->lastStrategyID)
			{
			case 5: // Cross only
				this->flagResetTimer.LoadInterval(leader5ExpireResetDelay);
				return;

			case 7: // Cross with henchmen
				this->flagResetTimer.LoadInterval(leader7ExpireResetDelay);
				return;
			}

			this->flagResetTimer.UpdateParameters(true, 1.f, 1.f); // vanilla-like
		}


		void ProcessCrossWreck()
		{
			switch (this->lastStrategyID)
			{
			case 5: // Cross only
				this->flagResetTimer.LoadInterval(leader5WreckResetDelay);
				return;

			case 7: // Cross with henchmen
				this->flagResetTimer.LoadInterval(leader7WreckResetDelay);
				return;
			}

			this->flagResetTimer.DisableInterval(); // vanilla-like
		}


		void ProcessCrossLoss()
		{
			switch (this->lastStrategyID)
			{
			case 5: // Cross only
				this->flagResetTimer.LoadInterval(leader5LostResetDelay);
				return;

			case 7: // Cross with henchmen
				this->flagResetTimer.LoadInterval(leader7LostResetDelay);
				return;
			}

			this->flagResetTimer.UpdateParameters(true, 1.f, 1.f); // vanilla-like
		}


		void UpdateFlagResetTimer()
		{
			std::string_view statusName;

			// Reset-timer selection
			switch (this->crossStatus)
			{
			case Status::EXPIRED:
				statusName = "(expire)";
				this->ProcessCrossExpiration();
				break;

			case Status::WRECKED:
				statusName = "(wreck)";
				this->ProcessCrossWreck();
				break;

			case Status::LOST:
				statusName = "(lost)";
				this->ProcessCrossLoss();
				break;

			default:
				return; // ACTIVE, PENDING
			}

			if (not flagResetTimer.IsSet())
				this->flagResetTimer.Start();

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
				Globals::EndSupportGoal(copVehicle);

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
				Globals::EndSupportGoal(this->crossVehicle);
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Cross now aggressive");
			}
			
			if (this->henchmenAggroTimer.HasExpired())
				this->MakeHenchmenAggro();
		}


		void ProcessAddedCross(const address copVehicle)
		{
			this->skipPriority = true;
			this->crossVehicle = copVehicle;
			this->flagResetTimer.Stop();

			this->expirationTimestamp = Globals::simulationTime;

			if (this->leaderStrategy)
			{
				this->lastStrategyID       = AsReference<int>  (this->leaderStrategy);
				this->expirationTimestamp += AsReference<float>(this->leaderStrategy + 0x8); // strategy duration

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Strategy ID now", this->lastStrategyID);
			}
			else this->lastStrategyID = 0;

			this->SetCrossStatus(Status::ACTIVE);

			// Aggro-timer selection
			switch (this->lastStrategyID)
			{
			case 5: // Cross only
				this->crossAggroTimer.LoadInterval(leader5CrossAggroDelay);
				break;

			case 7: // Cross with henchmen
				this->crossAggroTimer.LoadInterval(leader7CrossAggroDelay);
				break;

			default:
				this->crossAggroTimer.DisableInterval();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [LDR] LeaderStrategy", this->lastStrategyID, "in", pursuit);
			}

			this->crossAggroTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				if (this->crossAggroTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Cross aggro in", this->crossAggroTimer.GetLength());
			}
		}


		void ProcessAddedHenchman(const address copVehicle)
		{
			this->passiveHenchmenVehicles.insert(copVehicle);

			if (not this->henchmenAggroTimer.IsSet())
			{
				// Aggro-timer selection
				switch (this->lastStrategyID)
				{
				case 7: // Cross with henchmen
					this->henchmenAggroTimer.LoadInterval(leader7HenchAggroDelay);
					break;

				default:
					this->henchmenAggroTimer.DisableInterval();
				}

				this->henchmenAggroTimer.Start();

				if constexpr (Globals::loggingEnabled)
				{
					if (this->henchmenAggroTimer.IsIntervalEnabled())
						Globals::logger.Log(this->pursuit, "[LDR] Henchmen aggro in", this->henchmenAggroTimer.GetLength());
				}
			}
		}


	public:

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


		explicit LeaderManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			this->passiveHenchmenVehicles.reserve(2);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "LeaderManager");
		}


		~LeaderManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "LeaderManager");
		}


		void ReactToGameplay() override
		{
			this->CheckAggroTimers();
			this->CheckFlagResetTimer();
		}


		void ReactToHeatStateUpdate() override
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
				this->ProcessAddedCross(copVehicle);

			else
				this->ProcessAddedHenchman(copVehicle);
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

				// Cause of Cross despawn
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

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [LDR] LeaderOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Strategies.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Leader5:CrossAggro", leader5CrossAggroDelay);

		HeatParameters::Parse(parser, "Leader5:ExpireReset", leader5ExpireResetDelay);

		HeatParameters::Parse(parser, "Leader5:WreckReset", leader5WreckResetDelay);

		HeatParameters::Parse(parser, "Leader5:LostReset", leader5LostResetDelay);

		HeatParameters::Parse(parser, "Leader7:CrossAggro", leader7CrossAggroDelay);

		HeatParameters::Parse(parser, "Leader7:HenchmenAggro", leader7HenchAggroDelay);

		HeatParameters::Parse(parser, "Leader7:ExpireReset", leader7ExpireResetDelay);

		HeatParameters::Parse(parser, "Leader7:WreckReset", leader7WreckResetDelay);

		HeatParameters::Parse(parser, "Leader7:LostReset", leader7LostResetDelay);

		// Code modifications
		MemoryTools::MakeRangeNOP<0x42B6A2, 0x42B6B4>(); // Cross flag = 0
		MemoryTools::MakeRangeNOP<0x42402A, 0x424036>(); //              1
		MemoryTools::MakeRangeNOP<0x42B631, 0x42B643>(); //              2

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		if (
			leader5CrossAggroDelay    .isEnabled.current
			or leader5ExpireResetDelay.isEnabled.current
			or leader5WreckResetDelay .isEnabled.current
			or leader5LostResetDelay  .isEnabled.current
			or leader7CrossAggroDelay .isEnabled.current
			or leader7HenchAggroDelay. isEnabled.current
			or leader7ExpireResetDelay.isEnabled.current
			or leader7WreckResetDelay. isEnabled.current
			or leader7LostResetDelay  .isEnabled.current
		   )
		{
			Globals::logger.Log("    HEAT [LDR] LeaderOverrides");

			leader5CrossAggroDelay.Log("leader5CrossAggroDelay  ");

			leader5ExpireResetDelay.Log("leader5ExpireResetDelay ");

			leader5WreckResetDelay.Log("leader5WreckResetDelays ");

			leader5LostResetDelay.Log("leader5LostResetDelays  ");

			leader7CrossAggroDelay.Log("leader7CrossAggroDelay  ");

			leader7HenchAggroDelay.Log("leader7HenchAggroDelay  ");

			leader7ExpireResetDelay.Log("leader7ExpireResetDelay ");

			leader7WreckResetDelay.Log("leader7WreckResetDelays ");

			leader7LostResetDelay.Log("leader7LostResetDelays  ");
		}
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		leader5CrossAggroDelay.SetToHeatState(isRacing, heatLevel);

		leader5ExpireResetDelay.SetToHeatState(isRacing, heatLevel);

		leader5WreckResetDelay.SetToHeatState(isRacing, heatLevel);

		leader5LostResetDelay.SetToHeatState(isRacing, heatLevel);

		leader7CrossAggroDelay.SetToHeatState(isRacing, heatLevel);

		leader7HenchAggroDelay.SetToHeatState(isRacing, heatLevel);

		leader7ExpireResetDelay.SetToHeatState(isRacing, heatLevel);

		leader7WreckResetDelay.SetToHeatState(isRacing, heatLevel);

		leader7LostResetDelay.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}
}