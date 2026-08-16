#pragma once

#include <span>
#include <concepts>

#include "../../Common/Globals.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"



namespace PursuitFeatures
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Logging
	constexpr LogLiteral logTag  = "[PFT]";
	constexpr LogLiteral logName = "PursuitFeatures";





	// Reaction class -------------------------------------------------------------------------------------------------------------------------------

	class Reaction
	{
	public: // types

		enum class CopLabel
		{
			UNKNOWN,
			CHASER,
			HEAVY,
			LEADER,
			ROADBLOCK,
			HELICOPTER
		};


	protected: // members

		const address pursuit;


	protected: // methods

		explicit Reaction(const address pursuit) : pursuit(pursuit) {}


		Reaction(Reaction&&)      = delete;
		Reaction(const Reaction&) = delete;

		Reaction& operator=(Reaction&&)      = delete;
		Reaction& operator=(const Reaction&) = delete;


	public: // methods

		virtual ~Reaction() = default;


		virtual void ReactToGameplay                () {}
		virtual void ReactToHeatStateUpdate         () {}
		virtual void ReactToPursuitStartWithDelay   () {}
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





	// Searchable class -----------------------------------------------------------------------------------------------------------------------------

	template <class Feature>
	class Searchable
	{
	private: // members

		inline static RELEASE_CONSTINIT ModContainers::Set<Feature*> instances;


	protected: // methods

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
					Globals::LogError(logTag, "Registration failed:", instance);
			}
		}


		Searchable(Searchable&&)      = delete;
		Searchable(const Searchable&) = delete;

		Searchable& operator=(Searchable&&)      = delete;
		Searchable& operator=(const Searchable&) = delete;


		~Searchable()
		{
			const auto* const instance      = static_cast<const Feature*>(this);
			const bool        wasRegistered = this->instances.erase(instance);

			if constexpr (Globals::loggingEnabled)
			{
				if (not wasRegistered)
					Globals::LogError(logTag, "Unregistration failed:", instance);
			}
		}


		[[nodiscard]] static Feature* FindInstance(const address pursuit)
		{
			for (auto* const instance : Searchable::instances)
				if (instance->GetPursuit() == pursuit) return instance;

			if constexpr (Globals::loggingEnabled)
				Globals::LogError(logTag, "Lookup failed:", pursuit);

			return nullptr; // should never happen
		}


		[[nodiscard]] static std::span<Feature* const> GetInstances()
		{
			return Searchable::instances;
		}
	};





	// IntervalTimer class --------------------------------------------------------------------------------------------------------------------------

	class IntervalTimer
	{
	private: // members

		bool  isEnabled = false;
		float minLength = 1.f;
		float maxLength = 1.f;

		bool  isSet          = false;
		float length         = 0.f;
		float startTimestamp = 0.f;
		float endTimestamp   = 0.f;


	private: // methods

		void UpdateLength()
		{
			this->length       = Globals::prng.GenerateNumber<float>(this->minLength, this->maxLength);
			this->endTimestamp = this->startTimestamp + this->length;
		}


	public: // methods

		bool Start()
		{
			if (this->isSet)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogError(logTag, "Timer already set");

				return false; // should never happen
			}

			this->isSet          = true;
			this->startTimestamp = Globals::simulationTime;

			if (this->isEnabled)
				this->UpdateLength();

			return true;
		}


		bool Stop()
		{
			if (not this->isSet) return false;

			this->isSet = false;

			return true;
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
			if (not this->isEnabled) return;

			this->minLength = minLength;
			this->maxLength = maxLength;

			if (this->isSet)
				this->UpdateLength();
		}


		void LoadInterval(const HeatParameters::Interval<float>& interval)
		{
			this->UpdateParameters(Globals::playerHeatLevelKnown, interval.min.current, interval.max.current);
		}


		void LoadInterval(const HeatParameters::OptionalInterval<float>& optionalInterval)
		{
			const bool  isEnabled = (Globals::playerHeatLevelKnown and optionalInterval.isEnabled.current);
			const auto& interval  = optionalInterval.interval;

			this->UpdateParameters(isEnabled, interval.min.current, interval.max.current);
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