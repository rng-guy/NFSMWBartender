#pragma once

#include <array>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{

	// PursuitReaction class ------------------------------------------------------------------------------------------------------------------------

	class PursuitReaction
	{
	protected:

		const address pursuit;

		const int& pursuitStatus = *reinterpret_cast<int*>(this->pursuit + 0x218);

		
		address GetFromPursuitlevel
		(
			const vault  attributeKey,
			const size_t attributeIndex = 0
		)
			const
		{
			static const auto GetPursuitNode      = reinterpret_cast<address (__thiscall*)(address)>               (0x418E90);
			static const auto GetPursuitAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454810);

			const address node = GetPursuitNode(this->pursuit);
			return (node) ? GetPursuitAttribute(node, attributeKey, attributeIndex) : node;
		}


		static address GetPursuitVehicle(const address copVehicle)
		{
			const address copAIVehicle = *reinterpret_cast<address*>(copVehicle + 0x54);

			if constexpr (Globals::loggingEnabled)
			{
				if (not copAIVehicle)
					Globals::logger.Log("WARNING: [PFT] Invalid AIVehicle pointer for", copVehicle);
			}

			return (copAIVehicle) ? (copAIVehicle - 0x4C + 0x758) : 0x0;
		}


		static bool MakeVehicleBail(const address copVehicle)
		{
			const address pursuitVehicle = PursuitReaction::GetPursuitVehicle(copVehicle);

			static const auto StartFlee = reinterpret_cast<void (__thiscall*)(address)>(0x423370);

			if (pursuitVehicle)
				StartFlee(pursuitVehicle);

			return pursuitVehicle;
		}


		bool IsSearchModeActive() const
		{
			return (this->pursuitStatus == 2);
		}



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