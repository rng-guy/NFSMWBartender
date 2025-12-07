#pragma once

#include <array>
#include <string>
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
	struct Pair : public BasePair<T>
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



	template <typename T>
	requires std::is_arithmetic_v<T>
	struct Pair<T> : public BasePair<T>
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


		bool AnyNonzero() const
		{
			for (const bool forRaces : {false, true})
			{
				for (const T value : this->GetValues(forRaces))
					if (value) return true;
			}

			return false;
		}


		void Log(const char* const pairName) const
		{
			Globals::logger.LogLongIndent(pairName, this->current);
		}
	};



	template <>
	struct Pair<std::string> : public BasePair<std::string>
	{
		const char* current;


		explicit Pair(const char* const original) : current(original) {}


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
		Pair<T>    values     = Pair<T>   (T());


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);
			this->values    .SetToHeat(forRaces, heatLevel);
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
		{}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->minValues.SetToHeat(forRaces, heatLevel);
			this->maxValues.SetToHeat(forRaces, heatLevel);
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
		Pair<T>    minValues  = Pair<T>(T());
		Pair<T>    maxValues  = Pair<T>(T());


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
		const char* const    pairName,
		Pair<std::string>&   pair,
		const Globals::Class vehicleClass
	) {
		size_t numTotalReplaced = 0;

		for (const bool forRaces : {false, true})
		{
			size_t numReplaced = 0;

			for (const size_t heatLevel : heatLevels)
			{
				std::string& vehicle = pair.GetValues(forRaces)[heatLevel - 1];
				const vault  copType = Globals::GetVaultKey(vehicle.c_str());

				if (not Globals::VehicleClassMatches(copType, vehicleClass))
				{
					// With logging disabled, the compiler optimises "pairName" away
					if constexpr (Globals::loggingEnabled)
					{
						if (numReplaced == 0)
							Globals::logger.LogLongIndent(pairName, (forRaces) ? "(race)" : "(free-roam)");

						Globals::logger.LogLongIndent(' ', static_cast<int>(heatLevel), vehicle, "->", pair.current);
					}

					vehicle = pair.current;

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
	void Parse
	(
		Parser&                    parser,
		const std::string&         section,
		ParsingSetup<Pair<T>, T>&& setup
	) {
		parser.ParseFormatParameter<T>
		(
			section,
			configFormatRoam,
			setup.values.roam,
			setup.values.current,
			setup.lowerBound,
			setup.upperBound
		);

		setup.values.race = setup.values.roam;

		parser.ParseFormatParameter<T>
		(
			section,
			configFormatRace,
			setup.values.race,
			{},
			setup.lowerBound,
			setup.upperBound
		);
	}



	template <typename ...T>
	void Parse
	(
		Parser&                        parser,
		const std::string&             section,
		ParsingSetup<Pair<T>, T>&&  ...columns
	) {
		parser.ParseParameterTable<T...>
		(
			section,
			configFormatRoam,
			Format<T>
			(
				columns.values.roam,
				columns.values.current,
				columns.lowerBound,
				columns.upperBound
			)...
		);

		const Values<bool> isValids = parser.ParseParameterTable<T...>
		(
			section,
			configFormatRace,
			Format<T>
			(
				columns.values.race,
				{},
				columns.lowerBound,
				columns.upperBound
			)...
		);

		(
			[&]
			{
				for (const size_t heatLevel : heatLevels)
				{
					if (not isValids[heatLevel - 1])
						columns.values.race[heatLevel - 1] = columns.values.roam[heatLevel - 1];
				}
			}
		(), ...);
	}



	template <typename V, typename ...T>
	void Parse
	(
		Parser&                           parser,
		const std::string&                section,
		ParsingSetup<Interval<V>, V>&&    setup,
		ParsingSetup<Pair<T>, T>&&     ...columns
	) {
		Parse<V, V, T...>
		(
			parser, 
			section, 
			{setup.values.minValues, setup.lowerBound, setup.upperBound},
			{setup.values.maxValues, setup.lowerBound, setup.upperBound},
			std::forward<ParsingSetup<Pair<T>, T>>(columns)...
		);

		ValidateIntervals<V>(setup.values.minValues, setup.values.maxValues);
	}



	template <typename ...T>
	void ParseOptional
	(
		Parser&                        parser,
		const std::string&             section,
		Pair<bool>&                    isEnableds,
		ParsingSetup<Pair<T>, T>&&  ...columns
	) {
		for (const bool forRaces : {false, true})
		{
			isEnableds.GetValues(forRaces) = parser.ParseParameterTable<T...>
			(
				section,
				(forRaces) ? configFormatRace : configFormatRoam,
				Format<T>
				(
					columns.values.GetValues(forRaces),
					{},
					columns.lowerBound,
					columns.upperBound
				)...
			);
		}
	}



	template <typename V, typename ...T>
	void ParseOptional
	(
		Parser&                               parser,
		const std::string&                    section,
		ParsingSetup<OptionalPair<V>, V>&&    setup,
		ParsingSetup<Pair<T>, T>&&         ...columns
	) {
		ParseOptional<V, T...>
		(
			parser,
			section,
			setup.values.isEnableds,
			{setup.values.values, setup.lowerBound, setup.upperBound},
			std::forward<ParsingSetup<Pair<T>, T>>(columns)...
		);
	}



	template <typename V, typename ...T>
	void ParseOptional
	(
		Parser&                                   parser,
		const std::string&                        section,
		ParsingSetup<OptionalInterval<V>, V>&&    setup,
		ParsingSetup<Pair<T>, T>&&             ...columns
	) {
		ParseOptional<V, V, T...>
		(
			parser, 
			section, 
			setup.values.isEnableds,
			{setup.values.minValues, setup.lowerBound, setup.upperBound},
			{setup.values.maxValues, setup.lowerBound, setup.upperBound},
			std::forward<ParsingSetup<Pair<T>, T>>(columns)...
		);

		ValidateIntervals<V>(setup.values.minValues, setup.values.maxValues);
	}
}