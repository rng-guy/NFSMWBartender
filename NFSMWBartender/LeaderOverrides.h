#pragma once

#include "Globals.h"
#include "PursuitFeatures.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace LeaderOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool>  escapeResetEnableds (false);
	HeatParameters::Pair<float> minEscapeResetDelays(1.f);
	HeatParameters::Pair<float> maxEscapeResetDelays(1.f);

	HeatParameters::Pair<bool>  wreckResetEnableds(false);
	HeatParameters::Pair<float> minWreckResetDelays(1.f);
	HeatParameters::Pair<float> maxWreckResetDelays(1.f);





	// LeaderManager class --------------------------------------------------------------------------------------------------------------------------

	class LeaderManager : public PursuitFeatures::PursuitReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:

		enum class Status
		{
			INACTIVE,
			ACTIVE,
			ESCAPED,
			WRECKED
		};

		int* const leaderFlag = (int*)(this->pursuit + 0x164);

		address leaderVehicle = 0x0;
		Status  leaderStatus  = Status::INACTIVE;

		float lastChangeTimestamp = *(this->simulationTime);
		float nextResetTimestamp  = this->lastChangeTimestamp;


		bool HasTimerExpired() const
		{
			if (not this->IsHeatLevelKnown()) return false;
			return (*(this->simulationTime) >= this->nextResetTimestamp);
		}


		bool LeaderCanSpawn() const
		{
			switch (this->leaderStatus)
			{
			case Status::INACTIVE:
				return true;

			case Status::WRECKED:
				return (wreckResetEnableds.current) ? this->HasTimerExpired() : false;

			case Status::ESCAPED:
				return (escapeResetEnableds.current) ? this->HasTimerExpired() : false;
			}

			return false;
		}


		void GenerateResetTimestamp
		(
			const bool        enabled,
			const float       minDelay,
			const float       maxDelay,
			const char* const timerName
		) {
			if (not enabled) return;

			this->nextResetTimestamp = this->lastChangeTimestamp + Globals::prng.Generate<float>(minDelay, maxDelay);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[LEA] Reset in", this->nextResetTimestamp - *(this->simulationTime), timerName);
		}


		void UpdateResetTimestamp()
		{
			if (not this->IsHeatLevelKnown()) return;

			switch (this->leaderStatus)
			{
			case Status::WRECKED:
				this->GenerateResetTimestamp
				(
					wreckResetEnableds.current, 
					minWreckResetDelays.current, 
					maxWreckResetDelays.current, 
					"(wreck)"
				);
				break;

			case Status::ESCAPED:
				this->GenerateResetTimestamp
				(
					escapeResetEnableds.current, 
					minEscapeResetDelays.current, 
					maxEscapeResetDelays.current, 
					"(escape)"
				);
			}
		}


		void UpdateLeaderFlag() const
		{
			*(this->leaderFlag) = (this->leaderStatus == Status::ACTIVE) ? 1 : ((this->LeaderCanSpawn()) ? 0 : 2);
		}



	public:

		explicit LeaderManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) {}


		void UpdateOnGameplay() override
		{
			this->UpdateLeaderFlag();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateResetTimestamp();
			this->UpdateLeaderFlag();
		}


		void ProcessAddition
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (copLabel != CopLabel::LEADER) return;

			if (this->leaderStatus != Status::ACTIVE)
			{
				this->leaderVehicle        = copVehicle;
				this->leaderStatus         = Status::ACTIVE;
				this->lastChangeTimestamp = *(this->simulationTime);
			}
			
			this->UpdateLeaderFlag();
		}


		void ProcessRemoval
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (copLabel != CopLabel::LEADER) return;

			static bool (__thiscall* const IsDestroyed)(address) = (bool (__thiscall*)(address))0x688170;

			if ((this->leaderStatus == Status::ACTIVE) and (this->leaderVehicle == copVehicle))
			{
				this->leaderVehicle        = 0x0;
				this->leaderStatus         = (IsDestroyed(copVehicle)) ? Status::WRECKED : Status::ESCAPED;
				this->lastChangeTimestamp = *(this->simulationTime);

				this->UpdateResetTimestamp();
			}
			
			this->UpdateLeaderFlag();
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
			"Leader:EscapeReset", 
			escapeResetEnableds, 
			minEscapeResetDelays, 
			maxEscapeResetDelays
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

		escapeResetEnableds .SetToHeat(isRacing, heatLevel);
		minEscapeResetDelays.SetToHeat(isRacing, heatLevel);
		maxEscapeResetDelays.SetToHeat(isRacing, heatLevel);
		
		wreckResetEnableds .SetToHeat(isRacing, heatLevel);
		minWreckResetDelays.SetToHeat(isRacing, heatLevel);
		maxWreckResetDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [LEA] LeaderOverrides");

			Globals::logger.LogLongIndent("Escape", (escapeResetEnableds.current) ? "resets" : "blocks");
			
			if (escapeResetEnableds.current)
			{
				Globals::logger.LogLongIndent("minEscapeResetDelay     ", minEscapeResetDelays.current);
				Globals::logger.LogLongIndent("maxEscapeResetDelay     ", maxEscapeResetDelays.current);
			}

			Globals::logger.LogLongIndent("Wreck", (wreckResetEnableds.current) ? "resets" : "blocks");

			if (wreckResetEnableds.current)
			{
				Globals::logger.LogLongIndent("minWreckResetDelay      ", minWreckResetDelays.current);
				Globals::logger.LogLongIndent("maxWreckResetDelay      ", maxWreckResetDelays.current);
			}
		}
	}
}