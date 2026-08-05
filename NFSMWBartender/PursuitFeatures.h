#pragma once

#include <concepts>

#include "Globals.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace PursuitFeatures
{
	// Reaction class ------------------------------------------------------------------------------------------------------------------------

	class Reaction
	{
	public:

		// Class-specific enum
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


		explicit Reaction(const address pursuit) : pursuit(pursuit) {}


	public:

		explicit Reaction(Reaction&&)      = delete;
		explicit Reaction(const Reaction&) = delete;

		Reaction& operator=(Reaction&&)      = delete;
		Reaction& operator=(const Reaction&) = delete;


		virtual ~Reaction() = default;


		virtual void ReactToGameplay()                 {}
		virtual void ReactToHeatStateUpdate()          {}
		virtual void ReactToPursuitStartWithDelay()    {}
		virtual void ReactToHeatStateUpdateWithDelay() {}


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


		address GetPursuit() const
		{
			return this->pursuit;
		}
	};





	// Searchable class ----------------------------------------------------------------------------------------------------------------------

	template <class Feature>
	class Searchable
	{
	private:

		inline static RELEASE_CONSTINIT ModContainers::Set<Feature*> instances;


	protected:

		Searchable()
		{
			static_assert
			(
				requires (const Feature& feature)
				{
					{feature.GetPursuit()} -> std::same_as<address>;
				}, 
				"Feature must implement GetPursuit() -> address."
			);

			auto* const instance            = static_cast<Feature*>(this);
			const auto  [it, isNewInstance] = this->instances.insert(instance);

			if constexpr (Globals::loggingEnabled)
			{
				if (not isNewInstance)
					Globals::logger.Log("WARNING: [PFT] Registration failed:", instance);
			}
		}


		explicit Searchable(Searchable&&)      = delete;
		explicit Searchable(const Searchable&) = delete;

		Searchable& operator=(Searchable&&)      = delete;
		Searchable& operator=(const Searchable&) = delete;


		~Searchable()
		{
			const auto* const instance      = static_cast<const Feature*>(this);
			const bool        wasRegistered = this->instances.erase(instance);

			if constexpr (Globals::loggingEnabled)
			{
				if (not wasRegistered)
					Globals::logger.Log("WARNING: [PFT] Unregistration failed:", instance);
			}
		}


		static Feature* FindInstance(const address pursuit)
		{
			for (auto* const instance : Searchable::instances)
				if (instance->GetPursuit() == pursuit) return instance;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PFT] Lookup failed:", pursuit);

			return nullptr; // should never happen
		}
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
			this->UpdateParameters(Globals::playerHeatLevelKnown, interval.min.current, interval.max.current);
		}


		void LoadInterval(const HeatParameters::OptionalInterval<float>& interval)
		{
			this->UpdateParameters
			(
				interval.isEnabled.current and Globals::playerHeatLevelKnown,
				interval.min      .current, 
				interval.max      .current
			);
		}


		[[nodiscard]] bool IsSet() const
		{
			return this->isSet;
		}


		[[nodiscard]] bool IsIntervalEnabled() const
		{
			return this->isEnabled;
		}


		[[nodiscard]] float GetLength() const
		{
			return this->length;
		}


		[[nodiscard]] float GetTimeLeft() const
		{
			return this->endTimestamp - Globals::simulationTime;
		}


		[[nodiscard]] bool HasExpired() const
		{
			if (not this->isSet)     return false;
			if (not this->isEnabled) return false;

			return (this->GetTimeLeft() <= 0.f);
		}
	};
}