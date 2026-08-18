#pragma once

#include "../../Common/Globals.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "PursuitFeatures.hpp"



namespace LeaderOverrides
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[LDR]";
	constexpr LogLiteral logName = "LeaderOverrides";

	// Heat parameters
	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader5CrossAggroDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader5ExpireResetDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader5WreckResetDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader5LostResetDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader7CrossAggroDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader7HenchAggroDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader7ExpireResetDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader7WreckResetDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader7LostResetDelay, {1.f}); // seconds





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::Reaction
	{
	private: // types

		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST
		};


	private: // members

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

		inline static constexpr LogLiteral name = "LeaderManager";


	private: // methods

		void SetCrossStatus(const Status status)
		{
			LogLiteral statusName;

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
				Globals::LogFull(this->pursuit, logTag, "Cross flag now", this->crossFlag, statusName);
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

			this->flagResetTimer.UpdateParameters(/* isEnabled = */ true, 1.f, 1.f); // vanilla-like
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

			this->flagResetTimer.UpdateParameters(/* isEnabled = */ true, 1.f, 1.f); // vanilla-like
		}


		void UpdateFlagResetTimer()
		{
			LogLiteral statusName;

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
					Globals::LogFull(this->pursuit, logTag, "Flag reset", statusName, "in", this->flagResetTimer.GetTimeLeft());

				else Globals::LogFull(this->pursuit, logTag, "Flag reset", statusName, "suspended");
			}
		}


		void MakeHenchmenAggro()
		{
			this->henchmenAggroTimer.Stop();

			for (const address copVehicle : this->passiveHenchmenVehicles)
				Globals::EndSupportGoalOfVehicle(copVehicle);

			if constexpr (Globals::loggingEnabled)
			{
				if (not this->passiveHenchmenVehicles.empty())
					Globals::LogFull(this->pursuit, logTag, "Henchmen now aggressive");
			}

			this->passiveHenchmenVehicles.clear();
		}


		void CheckAggroTimers()
		{
			if (this->crossAggroTimer.HasExpired())
			{
				this->crossAggroTimer.Stop();
				Globals::EndSupportGoalOfVehicle(this->crossVehicle);
				
				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Cross now aggressive");
			}
			
			if (this->henchmenAggroTimer.HasExpired())
				this->MakeHenchmenAggro();
		}


		void ProcessAddedCross(const address copVehicle)
		{
			this->flagResetTimer.Stop();

			this->skipPriority        = true;
			this->crossVehicle        = copVehicle;
			this->expirationTimestamp = Globals::simulationTime;

			if (this->leaderStrategy)
			{
				this->lastStrategyID       = AsReference<int>  (this->leaderStrategy);
				this->expirationTimestamp += AsReference<float>(this->leaderStrategy + 0x8); // strategy duration

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Strategy ID now", this->lastStrategyID);
			}
			else this->lastStrategyID = 0; // should never happen

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

			default: // should never happen
				this->crossAggroTimer.DisableInterval();

				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "LeaderStrategy", this->lastStrategyID, "in", pursuit);
			}

			this->crossAggroTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				if (this->crossAggroTimer.IsIntervalEnabled())
					Globals::LogFull(this->pursuit, logTag, "Cross aggro in", this->crossAggroTimer.GetLength());
			}
		}


		void ProcessAddedHenchman(const address copVehicle)
		{
			this->passiveHenchmenVehicles.insert(copVehicle);

			if (this->henchmenAggroTimer.IsSet()) return;

			// Aggro-timer selection
			switch (this->lastStrategyID)
			{
			case 7: // Cross with henchmen
				this->henchmenAggroTimer.LoadInterval(leader7HenchAggroDelay);
				break;

			default: // should never happen
				this->henchmenAggroTimer.DisableInterval();
			}

			this->henchmenAggroTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				if (this->henchmenAggroTimer.IsIntervalEnabled())
					Globals::LogFull(this->pursuit, logTag, "Henchmen aggro in", this->henchmenAggroTimer.GetLength());
			}
		}


		void ProcessRemovedCross(const address copVehicle)
		{
			this->crossAggroTimer.Stop();

			this->crossVehicle = 0x0;

			Status newStatus = Status::LOST;

			if (Globals::IsVehicleDestroyed(copVehicle))
				newStatus = Status::WRECKED;

			else if (Globals::simulationTime >= this->expirationTimestamp)
				newStatus = Status::EXPIRED;

			this->SetCrossStatus(newStatus);

			this->UpdateFlagResetTimer();
			this->MakeHenchmenAggro   ();
		}


		void ProcessRemovedHenchman(const address copVehicle)
		{
			if (not this->passiveHenchmenVehicles.erase(copVehicle)) return;
			if (not this->passiveHenchmenVehicles.empty())           return;

			this->henchmenAggroTimer.Stop();
		}


	public: // members

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


	public: // methods

		explicit LeaderManager(const address pursuit) : PursuitFeatures::Reaction(pursuit) 
		{
			this->passiveHenchmenVehicles.reserve(2);

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('+', this, this->name);
		}


		~LeaderManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('-', this, this->name);
		}


		void ReactToGameplay() override
		{
			this->CheckAggroTimers   ();
			this->CheckFlagResetTimer();
		}


		void ReactToHeatStateUpdate() override
		{
			this->UpdateFlagResetTimer();
			this->CheckFlagResetTimer ();
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

			else this->ProcessAddedHenchman(copVehicle);
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
				this->ProcessRemovedCross(copVehicle);

			else this->ProcessRemovedHenchman(copVehicle);
		}
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

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



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		leader5CrossAggroDelay.SetToHeatState(state);

		leader5ExpireResetDelay.SetToHeatState(state);

		leader5WreckResetDelay.SetToHeatState(state);

		leader5LostResetDelay.SetToHeatState(state);

		leader7CrossAggroDelay.SetToHeatState(state);

		leader7HenchAggroDelay.SetToHeatState(state);

		leader7ExpireResetDelay.SetToHeatState(state);

		leader7WreckResetDelay.SetToHeatState(state);

		leader7LostResetDelay.SetToHeatState(state);
	}
}