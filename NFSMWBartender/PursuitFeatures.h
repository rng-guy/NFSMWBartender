#pragma once

#include "Globals.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{

	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	public:

		// Classs-specific enum
		enum class CopLabel
		{
			UNKNOWN,
			CHASER,
			HEAVY,
			LEADER,
			ROADBLOCK,
			HELICOPTER
		};



	protected:

		const address pursuit;


		explicit PursuitReaction(const address pursuit) : pursuit(pursuit) {};



	public:

		explicit PursuitReaction(const PursuitReaction&)   = delete;
		PursuitReaction& operator=(const PursuitReaction&) = delete;

		explicit PursuitReaction(PursuitReaction&&)   = delete;
		PursuitReaction& operator=(PursuitReaction&&) = delete;

		virtual ~PursuitReaction() = default;


		virtual void UpdateOnGameplay()       {}
		virtual void UpdateOnHeatChange()     {}
		virtual void UpdateOncePerPursuit()   {}
		virtual void UpdateOncePerHeatLevel() {}


		virtual void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) {};

		virtual void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) {};
	};





	// IntervalTimer class --------------------------------------------------------------------------------------------------------------------------

	class IntervalTimer
	{
	private:

		bool  isEnabled = false;
		float minLength = 1.f;
		float maxLength = 1.f;

		bool  isSet          = false;
		float length         = 0.f;
		float startTimestamp = 0.f;
		float endTimestamp   = 0.f;


		void UpdateLength()
		{
			this->length       = Globals::prng.GenerateNumber<float>(this->minLength, this->maxLength);
			this->endTimestamp = this->startTimestamp + this->length;
		}



	public:

		void Start()
		{
			if (not this->isSet)
			{
				this->isSet          = true;
				this->startTimestamp = Globals::simulationTime;

				if (this->isEnabled)
					this->UpdateLength();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PFT] IntervalTimer already set");
		}


		void Stop()
		{
			this->isSet = false;
		}


		void DisableInterval()
		{
			this->isEnabled = false;
		}


		void UpdateParameters
		(
			const bool  isEnabled,
			const float minLength,
			const float maxLength
		) {
			this->isEnabled = isEnabled;

			if (this->isEnabled)
			{
				this->minLength = minLength;
				this->maxLength = maxLength;

				if (this->isSet)
					this->UpdateLength();
			}
		}


		void LoadInterval(const HeatParameters::Interval<float>& interval)
		{
			this->UpdateParameters(true, interval.minValues.current, interval.maxValues.current);
		}


		void LoadInterval(const HeatParameters::OptionalInterval<float>& interval)
		{
			this->UpdateParameters
			(
				interval.isEnableds.current and Globals::playerHeatLevelKnown,
				interval.minValues.current, 
				interval.maxValues.current
			);
		}


		bool IsSet() const
		{
			return this->isSet;
		}


		bool IsIntervalEnabled() const
		{
			return this->isEnabled;
		}


		float GetLength() const
		{
			return this->length;
		}


		float GetTimeLeft() const
		{
			return (this->endTimestamp - Globals::simulationTime);
		}


		bool HasExpired() const
		{
			return (this->isSet and this->isEnabled and (this->GetTimeLeft() <= 0.f));
		}
	};
}