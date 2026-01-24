#pragma once

#include <array>
#include <tuple>
#include <string>
#include <variant>
#include <optional>
#include <algorithm>

#include "Globals.h"
#include "ConfigParser.h"



namespace HeatParameters
{
	
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat levels
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = static_cast<float>(maxHeatLevel);

	// Configuration files
	const std::string configFormatRoam   = "heat{:02}";
	const std::string configFormatRace   = "race{:02}";
	const std::string configPathMain     = "BartenderSettings/";
	const std::string configPathBasic    = configPathMain + "Basic/";
	const std::string configPathAdvanced = configPathMain + "Advanced/";





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser;

	template <typename T>
	using Values = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::FormatParameter<T, maxHeatLevel>;





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





	// HeatParameter classes ------------------------------------------------------------------------------------------------------------------------

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


		explicit Pair(const T original) : current(original) {}


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
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct OptionalPair
	{
		Pair<bool> isEnableds = Pair<bool>(false);

		Pair<T> values = Pair<T>(T());


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
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Interval
	{
		Pair<T> minValues;
		Pair<T> maxValues;


		explicit Interval
		(
			const T originalMin,
			const T originalMax
		) 
			: minValues(originalMin), maxValues(originalMax) 
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
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct OptionalInterval
	{
		Pair<bool> isEnableds = Pair<bool>(false);

		Pair<T> minValues = Pair<T>(T());
		Pair<T> maxValues = Pair<T>(T());


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
	};





	// Validation functions -------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	void ValidateIntervals
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



	bool ValidateVehicles
	(
		const char* const         pairName,
		PointerPair<std::string>& pointerPair,
		const Globals::Class      vehicleClass
	) {
		size_t numTotalReplaced = 0;

		for (const bool forRaces : {false, true})
		{
			size_t numReplaced = 0;
			auto&  vehicles    = pointerPair.GetValues(forRaces);

			for (const size_t heatLevel : heatLevels)
			{
				std::string& vehicle = vehicles[heatLevel - 1];
				const vault  copType = Globals::GetVaultKey(vehicle.c_str());

				if (not Globals::VehicleClassMatches(copType, vehicleClass))
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





	// Parsing types and functions ------------------------------------------------------------------------------------------------------------------

	template <typename T, typename V>
	struct ParsingSetup
	{
		T&                     values;
		const std::optional<V> lowerBound = {};
		const std::optional<V> upperBound = {};
	};



	template <typename T>
	auto ToRoamFormat(const ParsingSetup<Pair<T>, T>& setup)
	{
		return std::make_tuple(HeatParameters::Format<T>{setup.values.roam, setup.values.current, setup.lowerBound, setup.upperBound});
	}



	template <typename T>
	auto ToRoamFormat(const ParsingSetup<PointerPair<std::string>, std::string>& setup)
	{
		return std::make_tuple(HeatParameters::Format<std::string>{setup.values.roam, setup.values.current, {}, {}});
	}



	template <typename T>
	auto ToRoamFormat(const ParsingSetup<OptionalPair<T>, T>& setup)
	{
		return std::make_tuple(Format<T>{setup.values.values.roam, {}, setup.lowerBound, setup.upperBound});
	}



	template <typename T>
	auto ToRoamFormat(const ParsingSetup<Interval<T>, T>& setup)
	{
		return std::make_tuple
		(
			Format<T>{setup.values.minValues.roam, setup.values.minValues.current, setup.lowerBound, setup.upperBound},
			Format<T>{setup.values.maxValues.roam, setup.values.maxValues.current, setup.lowerBound, setup.upperBound}
		);
	}



	template <typename T>
	auto ToRoamFormat(const ParsingSetup<OptionalInterval<T>, T>& setup)
	{
		return std::make_tuple
		(
			Format<T>{setup.values.minValues.roam, {}, setup.lowerBound, setup.upperBound},
			Format<T>{setup.values.maxValues.roam, {}, setup.lowerBound, setup.upperBound}
		);
	}



	template <typename T>
	auto ToRaceFormat(const ParsingSetup<Pair<T>, T>& setup)
	{
		return std::make_tuple(HeatParameters::Format<T>(setup.values.race, {}, setup.lowerBound, setup.upperBound));
	}



	template <typename T>
	auto ToRaceFormat(const ParsingSetup<PointerPair<std::string>, std::string>& setup)
	{
		return std::make_tuple(HeatParameters::Format<std::string>{setup.values.race, {}, {}, {}});
	}



	template <typename T>
	auto ToRaceFormat(const ParsingSetup<OptionalPair<T>, T>& setup)
	{
		return std::make_tuple(Format<T>(setup.values.values.race, {}, setup.lowerBound, setup.upperBound));
	}



	template <typename T>
	auto ToRaceFormat(const ParsingSetup<Interval<T>, T>& setup)
	{
		return std::make_tuple
		(
			Format<T>(setup.values.minValues.race, {}, setup.lowerBound, setup.upperBound),
			Format<T>(setup.values.maxValues.race, {}, setup.lowerBound, setup.upperBound)
		);
	}



	template <typename T>
	auto ToRaceFormat(const ParsingSetup<OptionalInterval<T>, T>& setup)
	{
		return std::make_tuple
		(
			Format<T>{setup.values.minValues.race, {}, setup.lowerBound, setup.upperBound},
			Format<T>{setup.values.maxValues.race, {}, setup.lowerBound, setup.upperBound}
		);
	}



	template <typename T>
	auto ToSetup
	(
		Pair<T>&               data, 
		const std::optional<T> lowerBound = {}, 
		const std::optional<T> upperBound = {}
	) {
		return ParsingSetup<Pair<T>, T>(data, lowerBound, upperBound);
	}



	auto ToSetup(PointerPair<std::string>& data) 
	{
		return ParsingSetup<PointerPair<std::string>, std::string>(data, {}, {});
	}



	template <typename T>
	auto ToSetup
	(
		OptionalPair<T>&       data, 
		const std::optional<T> lowerBound = {}, 
		const std::optional<T> upperBound = {}
	) {
		return ParsingSetup<OptionalPair<T>, T>(data, lowerBound, upperBound);
	}



	template <typename T>
	auto ToSetup
	(
		Interval<T>&           data, 
		const std::optional<T> lowerBound = {}, 
		const std::optional<T> upperBound = {}
	) {
		return ParsingSetup<Interval<T>, T>(data, lowerBound, upperBound);
	}



	template <typename T>
	auto ToSetup
	(
		OptionalInterval<T>&   data,
		const std::optional<T> lowerBound = {},
		const std::optional<T> upperBound = {}
	) {
		return ParsingSetup<OptionalInterval<T>, T>(data, lowerBound, upperBound);
	}



	template <typename T, typename V>
	void InitialiseRaceValues(const ParsingSetup<T, V>& setup) {}



	template <typename T, typename V>
	void InitialiseRaceValues(const ParsingSetup<Pair<V>, V>& setup)
	{
		setup.values.race = setup.values.roam;
	}



	template <typename T, typename V>
	void InitialiseRaceValues(const ParsingSetup<PointerPair<std::string>, std::string>& setup)
	{
		setup.values.race = setup.values.roam;
	}



	template <typename T, typename V>
	void InitialiseRaceValues(const ParsingSetup<Interval<V>, V>& setup)
	{
		setup.values.minValues.race = setup.values.minValues.roam;
		setup.values.maxValues.race = setup.values.maxValues.roam;
	}



	template <typename T, typename V>
	void FinaliseIntervals
	(
		const ParsingSetup<T, V>& setup,
		const Values<bool>&       isRoamEnableds,
		const Values<bool>&       isRaceEnableds
	) 
	{
	}



	template <typename T, typename V>
	void FinaliseIntervals
	(
		const ParsingSetup<Interval<V>, V>& setup, 
		const Values<bool>&                 isRoamEnableds, 
		const Values<bool>&                 isRaceEnableds
	) {
		ValidateIntervals<V>(setup.values.minValues, setup.values.maxValues);
	}



	template <typename T, typename V>
	void FinaliseIntervals
	(
		const ParsingSetup<OptionalInterval<V>, V>& setup,
		const Values<bool>&                         isRoamEnableds,
		const Values<bool>&                         isRaceEnableds
	) {
		ValidateIntervals<V>(setup.values.minValues, setup.values.maxValues);

		setup.values.isEnableds.roam = isRoamEnableds;
		setup.values.isEnableds.race = isRaceEnableds;
	}



	template <typename ...T, typename ...V>
	void Parse(
		Parser&                       parser,
		const std::string&            section,
		const ParsingSetup<T, V>&& ...setups
	) {
		const auto isRoamEnableds = std::apply
		(
			[&](auto&& ...setups) -> Values<bool>
			{
				return parser.ParseParameterTable
				(
					section,
					configFormatRoam,
					std::forward<decltype(setups)>(setups)...
				);
			}, 
			std::move(std::tuple_cat(ToRoamFormat<V>(setups)...))
		);

		(InitialiseRaceValues<T, V>(setups), ...);

		const auto isRaceEnableds = std::apply
		(
			[&](auto&& ...setups) -> Values<bool>
			{
				return parser.ParseParameterTable
				(
					section,
					configFormatRace,
					std::forward<decltype(setups)>(setups)...
				);
			},
			std::move(std::tuple_cat(ToRaceFormat<V>(setups)...))
		);

		(FinaliseIntervals<T, V>(setups, isRoamEnableds, isRaceEnableds), ...);
	}
}