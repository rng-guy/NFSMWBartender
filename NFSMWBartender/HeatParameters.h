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

	// Vehicle validation
	constexpr vault pvehicle = 0x4A97EC8F;





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Parser = ConfigParser::Parser;

	template <typename T>
	using Values = std::array<T, maxHeatLevel>;

	template <typename T>
	using Format = ConfigParser::FormatParameter<T, maxHeatLevel>;





	// HeatParameter classes ------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	class BasePair
	{
	protected:

		BasePair() = default;



	public:

		Values<T> roam = {};
		Values<T> race = {};


		Values<T>& Get(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		const Values<T>& Get(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}
	};



	template <typename T>
	class Pair : public BasePair<T>
	{
	public:

		const T* current = &(this->roam[0]);


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = &(((forRaces) ? this->race : this->roam)[heatLevel - 1]);
		}
	};



	template <typename T>
	requires std::is_arithmetic_v<T>
	class Pair<T> : public BasePair<T>
	{
	public:

		T current;


		explicit Pair(const T initial) : current(initial) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = ((forRaces) ? this->race : this->roam)[heatLevel - 1];
		}
	};



	template <>
	class Pair<std::string> : public BasePair<std::string>
	{
	public:

		const char* current;


		explicit Pair(const char* const initial) : current(initial) {}


		void SetToHeat
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			this->current = (((forRaces) ? this->race : this->roam)[heatLevel - 1]).c_str();
		}
	};





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
				for (size_t heatLevel = 1; heatLevel <= maxHeatLevel; heatLevel++)
				{
					if (not isValids[heatLevel - 1])
						columns.pair.race[heatLevel - 1] = columns.pair.roam[heatLevel - 1];
				}
			}
		(), ...);
	}





	// Validation functions -------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	void CheckIntervals
	(
		Pair<T>&       minValues,
		const Pair<T>& maxValues
	) {
		for (const bool forRaces : {false, true})
		{
			Values<T>&       lowers = minValues.Get(forRaces);
			const Values<T>& uppers = maxValues.Get(forRaces);

			for (size_t heatLevel = 1; heatLevel <= maxHeatLevel; heatLevel++)
				lowers[heatLevel - 1] = std::min<T>(lowers[heatLevel - 1], uppers[heatLevel - 1]);
		}
	}



	bool VehicleClassMatches
	(
		const vault vehicleType,
		const bool  forHelicopter
	) {
		const address pointer = Globals::GetVaultParameter(0x4A97EC8F, vehicleType, 0x0EF6DDF2); // fetches "CLASS" from "pvehicle"

		if (pointer)
		{
			const vault vehicleClass = *((vault*)(pointer + 0x8));
			const bool  isHelicopter = (vehicleClass == 0xB80933AA); // checks if "CHOPPER"

			return (isHelicopter == forHelicopter);
		}

		return false;
	}



	size_t ReplaceInvalidVehicles
	(
		const char* const  pairName,
		Pair<std::string>& vehicles,
		const bool         forHelicopters
	) {
		size_t numTotalReplaced = 0;

		for (const bool forRaces : {false, true})
		{
			size_t numReplaced = 0;

			for (size_t heatLevel = 1; heatLevel <= maxHeatLevel; heatLevel++)
			{
				std::string& vehicle = vehicles.Get(forRaces)[heatLevel - 1];
				const vault  copType = Globals::GetVaultKey(vehicle.c_str());

				if (not VehicleClassMatches(copType, forHelicopters))
				{
					// With logging disabled, the compiler optimises "pairName" away
					if constexpr (Globals::loggingEnabled)
					{
						if (numReplaced == 0)
							Globals::logger.LogLongIndent(pairName, (forRaces) ? "(race)" : "(free-roam)");

						Globals::logger.LogLongIndent(' ', (int)heatLevel, vehicle, "->", vehicles.current);
					}

					vehicle = vehicles.current;

					numReplaced++;
				}
			}

			numTotalReplaced += numReplaced;
		}

		return numTotalReplaced;
	}
}