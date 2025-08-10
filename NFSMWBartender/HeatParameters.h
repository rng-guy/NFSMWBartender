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



	bool IsValidVehicle(const vault vehicleType)
	{
		static address* (__cdecl* const FindCollection)(vault, vault) = (address* (__cdecl*)(vault, vault))0x455FD0;

		return FindCollection(0x4A97EC8F, vehicleType); // searches "pvehicle" tree
	}



	bool IsValidVehicle(const std::string& vehicleName)
	{
		return IsValidVehicle(Globals::GetVaultKey(vehicleName.c_str()));
	}



	void ReplaceInvalidVehicles(Pair<std::string>& vehicles)
	{
		for (const bool forRaces : {false, true})
		{
			for (std::string& vehicleName : vehicles.Get(forRaces))
			{
				if (not IsValidVehicle(vehicleName))
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.LogLongIndent("Replacing", vehicleName, "with", vehicles.current);

					vehicleName = vehicles.current;
				}
			}
		}
	}
}