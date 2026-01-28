#pragma once

#include <array>
#include <tuple>
#include <string>
#include <utility>
#include <algorithm>

#include "Globals.h"
#include "ConfigParser.h"



namespace HeatParameters
{
	
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat parameters
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = static_cast<float>(maxHeatLevel);

	// Configuration files
	const std::string defaultValueHandle = "default";

	const std::string configFormatRoam   = "heat{:02}";
	const std::string configFormatRace   = "race{:02}";
	const std::string configPathMain     = "scripts/BartenderSettings/";
	const std::string configPathBasic    = configPathMain + "Basic/";
	const std::string configPathAdvanced = configPathMain + "Advanced/";





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser;

	template <typename T>
	using Bounds = ConfigParser::Bounds<T>;

	template <typename T>
	using Values = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::FormatParameter<T, maxHeatLevel>;

	template <typename T>
	using User = ConfigParser::UserParameter<T>;





	// Heat-level indices ---------------------------------------------------------------------------------------------------------------------------

	constexpr Values<size_t> GenerateHeatLevels()
	{
		Values<size_t> heatLevels   = {};
		size_t         currentLevel = 1;

		for (size_t& heatLevel : heatLevels)
			heatLevel = currentLevel++;

		return heatLevels;
	}

	constexpr Values<size_t> heatLevels = GenerateHeatLevels();





	// HeatParameter structs ------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct BasePair
	{
	protected:

		explicit BasePair() = default;



	public:

		Values<T> roam = {};
		Values<T> race = {};


		Values<T>& GetValues(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		const Values<T>& GetValues(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Pair : public BasePair<T>
	{
		T current;

		const Bounds<T> limits;


		explicit Pair
		(
			const T          original, 
			const Bounds<T>& limits    = {}
		) 
			: current(original), limits(limits)
		{
		}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = this->GetValues(forRaces)[heatLevel - 1];
		}


		T GetMinimum() const
		{
			T minimum = this->roam[0];

			for (const bool forRaces : {false, true})
			{
				const Values<T>& values = this->GetValues(forRaces);

				for (const T value : values)
					minimum = std::min<T>(minimum, value);
			}

			return minimum;
		}


		T GetMaximum() const
		{
			T maximum = this->roam[0];

			for (const bool forRaces : {false, true})
			{
				const Values<T>& values = this->GetValues(forRaces);

				for (const T value : values)
					maximum = std::max<T>(maximum, value);
			}

			return maximum;
		}


		void Log(const char* const pairName) const
		{
			Globals::logger.LogLongIndent(pairName, this->current);
		}


		auto ToFormat(const bool forRaces) 
		{
			if (forRaces)
				return std::make_tuple(Format<T>(this->race, {}, this->limits));

			else
				return std::make_tuple(Format<T>(this->roam, this->current, this->limits));
		}
	};



	template <>
	struct Pair<bool> : public BasePair<bool>
	{
		bool current;


		explicit Pair(const bool original) : current(original) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = this->GetValues(forRaces)[heatLevel - 1];
		}


		void Log(const char* const pairName) const
		{
			Globals::logger.LogLongIndent(pairName, this->current);
		}


		auto ToFormat(const bool forRaces)
		{
			if (forRaces)
				return std::make_tuple(Format<bool>(this->race));

			else
				return std::make_tuple(Format<bool>(this->roam, this->current));
		}
	};



	template <typename T>
	struct PointerPair : public BasePair<T>
	{
		const T* current = &(this->roam[0]);


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = &(this->GetValues(forRaces)[heatLevel - 1]);
		}
	};



	template <>
	struct PointerPair<std::string> : public BasePair<std::string>
	{
		const char* current;


		explicit PointerPair(const char* const original) : current(original) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = (this->GetValues(forRaces)[heatLevel - 1]).c_str();
		}


		void Log(const char* const pairName) const
		{
			Globals::logger.LogLongIndent(pairName, this->current);
		}


		auto ToFormat(const bool forRaces)
		{
			if (forRaces)
				return std::make_tuple(Format<std::string>(this->race));

			else
				return std::make_tuple(Format<std::string>(this->roam, this->current));
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct OptionalPair
	{
		Pair<bool> isEnableds = Pair<bool>(false);

		Pair<T> values;


		explicit OptionalPair(const Bounds<T>& limits = {}) : values(T(), limits) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
		}


		bool AnyEnabled() const
		{
			for (const bool forRaces : {false, true})
			{
				for (const T value : this->isEnableds.GetValues(forRaces))
					if (value) return true;
			}

			return false;
		}


		void Log(const char* const optionalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.LogLongIndent(optionalName, this->values.current);
		}


		auto ToFormat(const bool forRaces)
		{
			return std::make_tuple(Format<T>(this->values.GetValues(forRaces), {}, this->values.limits));
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Interval
	{
		Pair<T> minValues;
		Pair<T> maxValues;


		explicit Interval
		(
			const T          originalMin,
			const T          originalMax,
			const Bounds<T>& limits       = {}
		) 
			: minValues(originalMin, limits),
			  maxValues(originalMax, limits)
		{
		}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->minValues.SetToHeat(forRaces, heatLevel);
			this->maxValues.SetToHeat(forRaces, heatLevel);
		}


		T GetMinimum() const
		{
			return this->minValues.GetMinimum();
		}


		T GetMaximum() const
		{
			return this->maxValues.GetMaximum();
		}


		T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->minValues.current, this->maxValues.current);
		}


		void Log(const char* const intervalName) const
		{
			Globals::logger.LogLongIndent(intervalName, this->minValues.current, "to", this->maxValues.current);
		}


		auto ToFormat(const bool forRaces)
		{
			return std::tuple_cat(this->minValues.ToFormat(forRaces), this->maxValues.ToFormat(forRaces));
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct OptionalInterval
	{
		Pair<bool> isEnableds = Pair<bool>(false);

		Pair<T> minValues;
		Pair<T> maxValues;


		explicit OptionalInterval(const Bounds<T>& limits = {}) : minValues(T(), limits), maxValues(T(), limits) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->minValues .SetToHeat(forRaces, heatLevel);
			this->maxValues .SetToHeat(forRaces, heatLevel);
		}


		T GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<T>(this->minValues.current, this->maxValues.current);
		}


		bool AnyEnabled() const
		{
			for (const bool forRaces : {false, true})
			{
				for (const T value : this->isEnableds.GetValues(forRaces))
					if (value) return true;
			}

			return false;
		}


		void Log(const char* const intervalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.LogLongIndent(intervalName, this->minValues.current, "to", this->maxValues.current);
		}


		auto ToFormat(const bool forRaces)
		{
			return std::make_tuple
			(
				Format<T>(this->minValues.GetValues(forRaces), {}, this->minValues.limits),
				Format<T>(this->maxValues.GetValues(forRaces), {}, this->maxValues.limits)
			);
		}
	};





	// Validation functions -------------------------------------------------------------------------------------------------------------------------

	bool ValidateVehicles
	(
		const char* const         pairName,
		PointerPair<std::string>& pointerPair,
		const auto&               IsVehicleValid
	) {
		size_t numTotalReplaced = 0;

		for (const bool forRaces : {false, true})
		{
			size_t numReplaced = 0;
			auto&  vehicles    = pointerPair.GetValues(forRaces);

			for (const size_t heatLevel : heatLevels)
			{
				std::string& vehicle = vehicles[heatLevel - 1];

				if (not IsVehicleValid(Globals::StringToVaultKey(vehicle)))
				{
					// With logging disabled, the compiler optimises "pairName" away
					if constexpr (Globals::loggingEnabled)
					{
						if (numReplaced == 0)
							Globals::logger.LogLongIndent(pairName, (forRaces) ? "(race)" : "(free-roam)");

						Globals::logger.LogLongIndent(' ', static_cast<int>(heatLevel), vehicle, "->", pointerPair.current);
					}

					vehicle = pointerPair.current;

					++numReplaced;
				}
			}

			numTotalReplaced += numReplaced;
		}

		return (numTotalReplaced == 0);
	}





	// Auxiliary parsing functions ------------------------------------------------------------------------------------------------------------------

	namespace Auxiliary
	{

		template <typename T>
		void InitialiseRaceValues(T& parameter) {}


		template <typename T>
		void InitialiseRaceValues(Pair<T>& pair)
		{
			pair.race = pair.roam;
		}


		void InitialiseRaceValues(PointerPair<std::string>& pair)
		{
			pair.race = pair.roam;
		}


		template <typename T>
		void InitialiseRaceValues(Interval<T>& interval)
		{
			interval.minValues.race = interval.minValues.roam;
			interval.maxValues.race = interval.maxValues.roam;
		}



		template <typename T>
		void CheckIntervals
		(
			Pair<T>&       minValues,
			const Pair<T>& maxValues
		) {
			for (const bool forRaces : {false, true})
			{
				Values<T>&       lowers = minValues.GetValues(forRaces);
				const Values<T>& uppers = maxValues.GetValues(forRaces);

				for (const size_t heatLevel : heatLevels)
					lowers[heatLevel - 1] = std::min<T>(lowers[heatLevel - 1], uppers[heatLevel - 1]);
			}
		}



		template <typename T>
		void FinaliseIntervals
		(
			T&                  parameter,
			const Values<bool>& isRoamEnableds,
			const Values<bool>& isRaceEnableds
		) {}


		template <typename T>
		void FinaliseIntervals
		(
			Interval<T>&        interval,
			const Values<bool>& isRoamEnableds,
			const Values<bool>& isRaceEnableds
		) {
			CheckIntervals<T>(interval.minValues, interval.maxValues);
		}


		template <typename T>
		void FinaliseIntervals
		(
			OptionalInterval<T>& interval,
			const Values<bool>&  isRoamEnableds,
			const Values<bool>&  isRaceEnableds
		) {
			CheckIntervals<T>(interval.minValues, interval.maxValues);

			interval.isEnableds.roam = isRoamEnableds;
			interval.isEnableds.race = isRaceEnableds;
		}
	}





	// Generic parsing function ---------------------------------------------------------------------------------------------------------------------

	template <typename ...T>
	void Parse
	(
		Parser&               parser,
		const std::string&    section,
		T&                 ...parameters
	) {
		Values<bool> isRoamEnableds = {};
		Values<bool> isRaceEnableds = {};

		for (const bool forRaces : {false, true})
		{
			if (forRaces)
				(Auxiliary::InitialiseRaceValues(parameters), ...);

			auto& isEnableds = (forRaces) ? isRaceEnableds : isRoamEnableds;

			isEnableds = std::apply
			(
				[&](auto&& ...formats) -> Values<bool>
				{
					return parser.ParseTable<maxHeatLevel>
					(
						section,
						(forRaces) ? configFormatRace : configFormatRoam,
						std::forward<decltype(formats)>(formats)...
					);
				},
				std::move(std::tuple_cat(parameters.ToFormat(forRaces)...))
			);
		}

		(Auxiliary::FinaliseIntervals(parameters, isRoamEnableds, isRaceEnableds), ...);
	}
}