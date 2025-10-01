#pragma once

#include "Globals.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	const auto   IsVehicleDestroyed = (bool (__thiscall*)(address))0x688170;
	const float& simulationTime     = *((float*)0x9885D8);

	bool heatLevelKnown = false;





	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	protected:

		const address pursuit;
		
		explicit PursuitReaction(const address pursuit) : pursuit(pursuit) {};



	public:

		virtual ~PursuitReaction() = default;


		virtual void UpdateOnGameplay()       {}
		virtual void UpdateOnHeatChange()     {}
		virtual void UpdateOncePerPursuit()   {}
		virtual void UpdateOncePerHeatLevel() {}


		enum class CopLabel
		{
			UNKNOWN,
			CHASER,
			HEAVY,
			LEADER,
			ROADBLOCK,
			HELICOPTER
		};

		virtual void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) = 0;

		virtual void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) = 0;
	};





	// IntervalTimer class --------------------------------------------------------------------------------------------------------------------------

	class IntervalTimer
	{
	private:

		float length         = 0.f;
		float startTimestamp = 0.f;
		float endTimestamp   = 0.f;

		const bool&  isEnabled;
		const float& minDelay;
		const float& maxDelay;


		void UpdateStart()
		{
			this->startTimestamp = simulationTime;
		}


		void UpdateLength()
		{
			this->length = Globals::prng.GenerateNumber<float>(this->minDelay, this->maxDelay);
		}


		void UpdateEnd()
		{
			this->endTimestamp = this->startTimestamp + this->length;
		}



	public:

		explicit IntervalTimer
		(
			const HeatParameters::Pair<bool>&  isEnableds,
			const HeatParameters::Pair<float>& minDelays,
			const HeatParameters::Pair<float>& maxDelays
		) 
			: isEnabled(isEnableds.current), minDelay(minDelays.current), maxDelay(maxDelays.current)
		{}


		bool IsEnabled() const
		{
			return (heatLevelKnown and this->isEnabled);
		}


		enum class Setting
		{
			START,
			LENGTH,
			ALL
		};

		void Update(const Setting value)
		{
			switch (value)
			{
			case Setting::START:
				this->UpdateStart();
				break;

			case Setting::LENGTH:
				this->UpdateLength();
				break;

			case Setting::ALL:
				this->UpdateStart();
				this->UpdateLength();
			}

			this->UpdateEnd();
		}


		bool HasExpired() const
		{
			return (this->IsEnabled()) ? (simulationTime >= this->endTimestamp) : false;
		}


		float GetLength() const
		{
			return (this->IsEnabled()) ? this->length : 0.f;
		}


		float TimeLeft() const
		{
			return (this->IsEnabled()) ? this->endTimestamp - simulationTime : 0.f;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool MakeVehicleFlee(const address copVehicle)
	{
		static const auto StartFlee    = (void (__thiscall*)(address))0x423370;
		const address     copAIVehicle = *((address*)(copVehicle + 0x54));

		if (copAIVehicle)
			StartFlee(copAIVehicle + 0x70C);

		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [PFT] Invalid AIVehicle pointer for", copVehicle);

		return copAIVehicle;
	}
}