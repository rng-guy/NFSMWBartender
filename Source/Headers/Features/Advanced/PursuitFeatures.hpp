#pragma once

#include <span>
#include <optional>
#include <concepts>

#include "../../Common/Globals.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"



namespace PursuitFeatures
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	// Logging
	constexpr Globals::LogLiteral logTag  = "[PFT]";
	constexpr Globals::LogLiteral logName = "PursuitFeatures";





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

		const address pursuit; // pursuit-locked and immobile


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


		[[nodiscard]] address GetPursuit() const
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

			if (not isNewInstance)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Registration failed:", instance);

				ASSERT_UNREACHABLE;
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
			
			if (not wasRegistered)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Unregistration failed:", instance);

				ASSERT_UNREACHABLE;
			}
		}


		[[nodiscard]] static Feature* FindInstance(const address pursuit)
		{
			for (auto* const instance : Searchable::instances)
			{
				if (instance->GetPursuit() == pursuit) return instance;
			}

			return nullptr;
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

		std::optional<float> startTimestamp;
		std::optional<float> endTimestamp;

		bool isIntervalEnabled = false;

		float minLength = 0.f;
		float maxLength = 0.f;


	private: // methods

		void UpdateEndTimestamp()
		{
			if (this->startTimestamp and this->isIntervalEnabled)
			{
				const float length = Globals::pRNG.GenerateNumber<float>(this->minLength, this->maxLength);
				this->endTimestamp = *(this->startTimestamp) + length;
			}
			else this->endTimestamp.reset();
		}


		void UpdateInterval
		(
			const bool  isEnabled,
			const float minLength,
			const float maxLength
		) {
			this->isIntervalEnabled = isEnabled;

			this->minLength = minLength;
			this->maxLength = maxLength;

			this->UpdateEndTimestamp();
		}


		[[nodiscard]] float ComputeTimeLeft() const
		{
			return *(this->endTimestamp) - Globals::simulationTime;
		}


	public: // methods

		void SetStartTimestamp()
		{
			this->startTimestamp = Globals::simulationTime;

			this->UpdateEndTimestamp();
		}


		void SetStartTimestampIfNone()
		{
			if (this->startTimestamp) return;

			this->SetStartTimestamp();
		}


		void ClearStartTimestamp()
		{
			this->startTimestamp.reset();

			this->UpdateEndTimestamp();
		}


		[[nodiscard]] bool HasStartTimestamp() const
		{
			return this->startTimestamp.has_value();
		}


		[[nodiscard]] std::optional<float> GetStartTimestamp() const
		{
			return this->startTimestamp;
		}


		void LoadInterval
		(
			const float minLength,
			const float maxLength
		) {
			this->UpdateInterval
			(
				/* isEnabled = */ true, 
				minLength, 
				maxLength
			);
		}


		void LoadInterval(const HeatParameters::Interval<float>& interval)
		{
			this->LoadInterval
			(
				interval.min.current, 
				interval.max.current
			);
		}


		void LoadInterval(const HeatParameters::OptionalInterval<float>& optionalInterval)
		{
			this->UpdateInterval
			(
				optionalInterval.isEnabled   .current,
				optionalInterval.interval.min.current, 
				optionalInterval.interval.max.current
			);
		}


		void DisableInterval()
		{
			this->isIntervalEnabled = false;

			this->UpdateEndTimestamp();
		}


		[[nodiscard]] bool IsIntervalEnabled() const
		{
			return this->isIntervalEnabled;
		}


		[[nodiscard]] bool HasEndTimestamp() const
		{
			return this->endTimestamp.has_value();
		}


		[[nodiscard]] std::optional<float> GetEndTimestamp() const
		{
			return this->endTimestamp;
		}


		[[nodiscard]] std::optional<float> GetLength() const
		{
			if (not this->startTimestamp) return std::nullopt;
			if (not this->endTimestamp)   return std::nullopt;

			return *(this->endTimestamp) - *(this->startTimestamp);
		}


		[[nodiscard]] std::optional<float> GetTimeLeft() const
		{
			if (not this->endTimestamp) return std::nullopt;
			return this->ComputeTimeLeft();
		}


		[[nodiscard]] bool HasExpired() const
		{
			if (not this->endTimestamp) return false;
			return (this->ComputeTimeLeft() <= 0.f);
		}


		void Log(const Globals::LogLiteral actionName) const
		{
			if (const auto length = this->GetLength())
				Globals::LogPlain(actionName, "in", *length);

			else Globals::LogPlain(actionName, "suspended");
		}
	};
}