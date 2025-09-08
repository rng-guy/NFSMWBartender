#pragma once

#include "Globals.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	const auto         IsVehicleDestroyed = (bool (__thiscall*)(address))0x688170;
	const float* const simulationTime     = (float*)0x9885D8;





	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	private:

		inline static bool isHeatLevelKnown = false;



	protected:

		const address pursuit;
		
		explicit PursuitReaction(const address pursuit) : pursuit(pursuit) {};

		friend void SetToHeat(const bool, const size_t);
		friend void ResetState();



	public:

		virtual ~PursuitReaction() = default;


		virtual void UpdateOnGameplay()       {}
		virtual void UpdateOnHeatChange()     {}
		virtual void UpdateOnceInPursuit()    {}
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


		static bool IsHeatLevelKnown() 
		{
			return PursuitReaction::isHeatLevelKnown;
		}
	};





	// IntervalTimer class --------------------------------------------------------------------------------------------------------------------------

	class IntervalTimer
	{
	protected:

		float length         = 0.f;
		float startTimestamp = 0.f;
		float endTimestamp   = 0.f;

		const bool*  const isEnabled;
		const float* const minDelay;
		const float* const maxDelay;


		void UpdateStart()
		{
			this->startTimestamp = *simulationTime;
		}


		void UpdateLength()
		{
			this->length = Globals::prng.Generate<float>(*(this->minDelay), *(this->maxDelay));
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
			: isEnabled(&(isEnableds.current)), minDelay(&(minDelays.current)), maxDelay(&(maxDelays.current))
		{}


		bool IsEnabled() const
		{
			return (PursuitReaction::IsHeatLevelKnown() and *(this->isEnabled));
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
			return (this->IsEnabled()) ? (*simulationTime >= this->endTimestamp) : false;
		}


		float GetLength() const
		{
			return (this->IsEnabled()) ? this->length : 0.f;
		}


		float TimeLeft() const
		{
			return (this->IsEnabled()) ? this->endTimestamp - *simulationTime : 0.f;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool MakeVehicleFlee(const address copVehicle)
	{
		static const auto StartFlee    = (void (__thiscall*)(address))0x423370;
		const address     copAIVehicle = *((address*)(copVehicle + 0x54));

		if (copAIVehicle)
			StartFlee(copAIVehicle + 0x70C);

		return copAIVehicle;
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		PursuitReaction::isHeatLevelKnown = true;
	}



	void ResetState() 
	{
		PursuitReaction::isHeatLevelKnown = false;
	}
}