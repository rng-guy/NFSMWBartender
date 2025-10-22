#pragma once

#include <array>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool heatLevelKnown = false;
	
	// Data pointers
	const float& simulationTime = *((float*)0x9885D8);

	// Function pointers
	const auto GetVehicleName     = (const char* (__thiscall*)(address))0x688090;
	const auto IsVehicleDestroyed = (bool        (__thiscall*)(address))0x688170;

	// Scoped aliases
	using PursuitCache = MemoryTools::PointerCache<0xD0, 3>;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool MakeVehicleBail(const address copVehicle)
	{
		static const auto StartFlee    = (void (__thiscall*)(address))0x423370;
		const address     copAIVehicle = *((address*)(copVehicle + 0x54));

		if (copAIVehicle)
			StartFlee(copAIVehicle + 0x70C);

		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [PFT] Invalid AIVehicle pointer for", copVehicle);

		return copAIVehicle;
	}





	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	protected:

		const address pursuit;

		

	public:

		explicit PursuitReaction(const address pursuit) : pursuit(pursuit) {};

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
				this->startTimestamp = simulationTime;

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


		void LoadInterval(const HeatParameters::OptionalInterval& interval)
		{
			this->isEnabled = (heatLevelKnown and interval.isEnableds.current);

			if (this->isEnabled)
			{
				this->minLength = interval.minValues.current;
				this->maxLength = interval.maxValues.current;

				if (this->isSet)
					this->UpdateLength();
			}
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
			return (this->endTimestamp - simulationTime);
		}


		bool HasExpired() const
		{
			return (this->isSet and this->isEnabled and (this->GetTimeLeft() <= 0.f));
		}
	};
}