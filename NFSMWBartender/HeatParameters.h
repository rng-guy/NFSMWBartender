#pragma once

#include <array>
#include <string>
#include <concepts>
#include <optional>
#include <algorithm>

#include "Globals.h"
#include "ConfigParser.h"



namespace HeatParameters
{
	
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat levels
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = (float)maxHeatLevel;

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

		BasePair() = default;



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


		size_t Validate
		(
			const char* const           pairName,
			const Globals::VehicleClass vehicleClass
		) {
			size_t numTotalReplaced = 0;

			for (const bool forRaces : {false, true})
			{
				size_t numReplaced = 0;

				for (const size_t heatLevel : heatLevels)
				{
					std::string& vehicle = this->GetValues(forRaces)[heatLevel - 1];
					const vault  copType = Globals::GetVaultKey(vehicle.c_str());

					if (not Globals::VehicleClassMatches(copType, vehicleClass))
					{
						// With logging disabled, the compiler optimises "pairName" away
						if constexpr (Globals::loggingEnabled)
						{
							if (numReplaced == 0)
								Globals::logger.LogLongIndent(pairName, (forRaces) ? "(race)" : "(free-roam)");

							Globals::logger.LogLongIndent(' ', (int)heatLevel, vehicle, "->", this->current);
						}

						vehicle = this->current;

						++numReplaced;
					}
				}

				numTotalReplaced += numReplaced;
			}

			return numTotalReplaced;
		}
	};



	struct OptionalInterval
	{
		Pair<bool>  isEnableds = Pair<bool>(false);
		Pair<float> minValues  = Pair<float>(1.f);
		Pair<float> maxValues  = Pair<float>(1.f);


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->isEnableds.SetToHeat(forRaces, heatLevel);

			this->minValues.SetToHeat(forRaces, heatLevel);
			this->maxValues.SetToHeat(forRaces, heatLevel);
		}


		float GetRandomValue() const
		{
			return Globals::prng.GenerateNumber<float>(this->minValues.current, this->maxValues.current);
		}


		void Log(const char* const intervalName) const
		{
			if (this->isEnableds.current)
				Globals::logger.LogLongIndent(intervalName, this->minValues.current, "to", this->maxValues.current);
		}
	};





	// Validation functions -------------------------------------------------------------------------------------------------------------------------

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





	// Parsing types and functions ------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct ParsingSetup
	{
		Pair<T>&               pair;
		const std::optional<T> lowerBound = {};
		const std::optional<T> upperBound = {};
	};



	template <typename T>
	void Parse
	(
		Parser&            parser,
		const std::string& section,
		ParsingSetup<T>&&  setup
	) {
		parser.ParseFormatParameter<T>
		(
			section,
			configFormatRoam,
			setup.pair.roam,
			setup.pair.current,
			setup.lowerBound,
			setup.upperBound
		);

		setup.pair.race = setup.pair.roam;

		parser.ParseFormatParameter<T>
		(
			section,
			configFormatRace,
			setup.pair.race,
			{},
			setup.lowerBound,
			setup.upperBound
		);
	}



	template <typename ...T>
	void Parse
	(
		Parser&               parser,
		const std::string&    section,
		ParsingSetup<T>&&  ...columns
	) {
		parser.ParseParameterTable<T...>
		(
			section,
			configFormatRoam,
			Format<T>
			(
				columns.pair.roam,
				columns.pair.current,
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
				columns.pair.race,
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
						columns.pair.race[heatLevel - 1] = columns.pair.roam[heatLevel - 1];
				}
			}
		(), ...);
	}



	template <typename ...T>
	void ParseOptional
	(
		Parser&               parser,
		const std::string&    section,
		Pair<bool>&           isEnableds,
		ParsingSetup<T>&&  ...columns
	) {
		for (const bool forRaces : {false, true})
		{
			isEnableds.GetValues(forRaces) = parser.ParseParameterTable<T...>
			(
				section,
				(forRaces) ? configFormatRace : configFormatRoam,
				Format<T>
				(
					columns.pair.GetValues(forRaces),
					{},
					columns.lowerBound,
					columns.upperBound
				)...
			);
		}
	}



	bool Parse
	(
		Parser&            parser,
		const std::string& section,
		OptionalInterval&  interval
	) {
		ParseOptional<float, float>(parser, section, interval.isEnableds, {interval.minValues, 1.f}, {interval.maxValues, 1.f});

		if (not interval.isEnableds.AnyNonzero()) return false;
		
		CheckIntervals<float>(interval.minValues, interval.maxValues);

		return true;
	}
}