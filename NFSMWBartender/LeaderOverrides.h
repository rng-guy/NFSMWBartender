#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"
#include "PursuitFeatures.h"



namespace LeaderOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::OptionalInterval crossAggroDelays;
	HeatParameters::OptionalInterval henchmenAggroDelays;

	HeatParameters::OptionalInterval expireResetDelays;
	HeatParameters::OptionalInterval wreckResetDelays;
	HeatParameters::OptionalInterval lostResetDelays;





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

		address crossVehicle        = 0x0;
		Status  crossStatus         = Status::PENDING;
		float   expirationTimestamp = PursuitFeatures::simulationTime;

		int&  crossFlag    = *((int*)(this->pursuit + 0x164));
		bool& skipPriority = *((bool*)(this->pursuit + 0x214));

		const address& leaderStrategy = *((address*)(this->pursuit + 0x198));

		HashContainers::AddressSet passiveHenchmenVehicles;

		PursuitFeatures::IntervalTimer flagResetTimer;
		PursuitFeatures::IntervalTimer crossAggroTimer;
		PursuitFeatures::IntervalTimer henchmenAggroTimer;


		void SetCrossFlag(const int value) const
		{
			if ((value == 0) or (value == 1) or (value == 2))
				this->crossFlag = value;

			if constexpr (Globals::loggingEnabled)
			{
				const char* actionName = nullptr;

				switch (value)
				{
				case 0:
					actionName = "(available)";
					break;

				case 1:
					actionName = "(active)";
					break;

				case 2:
					actionName = "(blocked)";
				}

				if (actionName)
					Globals::logger.Log(this->pursuit, "[LDR] Cross flag now", value, actionName);

				else
					Globals::logger.Log("WARNING: [LDR] Invalid flag value", value, "in", this->pursuit);
			}
		}


		void CheckFlagResetTimer()
		{
			if (not this->flagResetTimer.HasExpired()) return;

			this->flagResetTimer.Stop();

			this->crossStatus = Status::PENDING;
			this->SetCrossFlag(0);
		}


		void UpdateFlagResetTimer()
		{
			switch (this->crossStatus)
			{
			case Status::EXPIRED:
				this->flagResetTimer.LoadInterval(expireResetDelays);
				break;

			case Status::WRECKED:
				this->flagResetTimer.LoadInterval(wreckResetDelays);
				break;

			case Status::LOST:
				this->flagResetTimer.LoadInterval(lostResetDelays);
				break;

			default:
				return;
			}

			if (not flagResetTimer.IsSet())
				this->flagResetTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				const char* actionName = nullptr;

				switch (this->crossStatus)
				{
				case Status::EXPIRED:
					actionName = "(expire)";
					break;

				case Status::WRECKED:
					actionName = "(wreck)";
					break;

				case Status::LOST:
					actionName = "(lost)";
				}

				if (actionName)
				{
					if (this->flagResetTimer.IsIntervalEnabled())
						Globals::logger.Log(this->pursuit, "[LDR] Flag reset", actionName, "in", this->flagResetTimer.GetTimeLeft());

					else
						Globals::logger.Log(this->pursuit, "[LDR] Flag reset", actionName, "suspended");
				}
				else Globals::logger.Log("WARNING: [LDR] Invalid status in timer update", (int)(this->crossStatus), "in", this->pursuit);
			}
		}


		void StartCrossAggroTimer()
		{
			if (this->crossAggroTimer.IsSet()) return;

			this->crossAggroTimer.LoadInterval(crossAggroDelays);
			this->crossAggroTimer.Start();
	
			if constexpr (Globals::loggingEnabled)
			{
				if (this->crossAggroTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Cross aggro in", this->crossAggroTimer.GetLength());
			}
		}


		void StartHenchmenAggroTimer()
		{
			if (this->henchmenAggroTimer.IsSet()) return;

			this->henchmenAggroTimer.LoadInterval(henchmenAggroDelays);
			this->henchmenAggroTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				if (this->henchmenAggroTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[LDR] Henchmen aggro in", this->henchmenAggroTimer.GetLength());
			}
		}


		static bool MakeVehicleAggro(const address copVehicle)
		{
			static const auto SetSupportGoal = (void (__thiscall*)(address, vault))0x409850;
			const address     copAIVehicle   = *((address*)(copVehicle + 0x54));

			if (copAIVehicle)
				SetSupportGoal(copAIVehicle + 0x70C, 0x0);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [LDR] Invalid AIVehicle pointer for", copVehicle);

			return copAIVehicle;
		}


		void MakeHenchmenAggro()
		{
			this->henchmenAggroTimer.Stop();

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
			if (this->crossAggroTimer.HasExpired())
			{
				this->crossAggroTimer.Stop();
				this->MakeVehicleAggro(this->crossVehicle);
				
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[LDR] Cross now aggressive");
			}
			
			if (this->henchmenAggroTimer.HasExpired())
				this->MakeHenchmenAggro();
		}


		void UpdateExpirationTimestamp()
		{
			this->expirationTimestamp = PursuitFeatures::simulationTime;

			if (this->leaderStrategy)
				this->expirationTimestamp += *((float*)(this->leaderStrategy + 0x8));

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [LDR] Invalid duration pointer in", this->pursuit);
		}



	public:

		explicit LeaderManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "LeaderManager");
		}


		~LeaderManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "LeaderManager");
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
				this->flagResetTimer.Stop();

				this->crossVehicle = copVehicle;
				this->crossStatus  = Status::ACTIVE;
				this->skipPriority = true;

				this->UpdateExpirationTimestamp();
				this->StartCrossAggroTimer();
				this->SetCrossFlag(1);
			}
			else
			{
				this->passiveHenchmenVehicles.insert(copVehicle);
				this->StartHenchmenAggroTimer();
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
				this->crossAggroTimer.Stop();
				this->crossVehicle = 0x0;

				if (PursuitFeatures::IsVehicleDestroyed(copVehicle))
					this->crossStatus = Status::WRECKED;
				
				else if (PursuitFeatures::simulationTime > this->expirationTimestamp)
					this->crossStatus = Status::EXPIRED;
				
				else				
					this->crossStatus = Status::LOST;
				
				this->SetCrossFlag(2);
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
		parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini");

		// Heat parameters
		HeatParameters::Parse<>(parser, "Leader:CrossAggro",    crossAggroDelays);
		HeatParameters::Parse<>(parser, "Leader:HenchmenAggro", henchmenAggroDelays);

		HeatParameters::Parse<>(parser, "Leader:ExpireReset", expireResetDelays);
		HeatParameters::Parse<>(parser, "Leader:WreckReset",  wreckResetDelays);
		HeatParameters::Parse<>(parser, "Leader:LostReset",   lostResetDelays);

		// Code caves
		MemoryTools::WriteToAddressRange(0x90, 0x42B6A2, 0x42B6B4); // Cross flag = 0
		MemoryTools::WriteToAddressRange(0x90, 0x42402A, 0x424036); // Cross flag = 1
		MemoryTools::WriteToAddressRange(0x90, 0x42B631, 0x42B643); // Cross flag = 2

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		if (
			crossAggroDelays.isEnableds.current
			or henchmenAggroDelays.isEnableds.current
			or expireResetDelays.isEnableds.current
			or wreckResetDelays.isEnableds.current
			or lostResetDelays.isEnableds.current
		   )
		{
			Globals::logger.Log("    HEAT [LDR] LeaderOverrides");

			crossAggroDelays   .Log("crossAggroDelay         ");
			henchmenAggroDelays.Log("henchmenAggroDelay      ");

			expireResetDelays.Log("expireResetDelay        ");
			wreckResetDelays .Log("wreckResetDelay         ");
			lostResetDelays  .Log("lostResetDelay          ");
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		crossAggroDelays   .SetToHeat(isRacing, heatLevel);
		henchmenAggroDelays.SetToHeat(isRacing, heatLevel);
		
		expireResetDelays.SetToHeat(isRacing, heatLevel);
		wreckResetDelays .SetToHeat(isRacing, heatLevel);
		lostResetDelays  .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}