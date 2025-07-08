#pragma once

#include <Windows.h>
#include <optional>
#include <fstream>
#include <memory>
#include <format>
#include <string>

#include "ConfigParser.h"

#undef min
#undef max

using address = DWORD;
using hash    = DWORD;
using key     = DWORD;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Heat levels
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = (float)maxHeatLevel;
	
	// Logging
	constexpr bool loggingEnabled = false;

	std::unique_ptr<std::fstream> logStream;

	const std::string logFile       = "BartenderLog.txt";
	const std::string logIndent     = "        ";
	const std::string logLongIndent = "              ";

	const std::string configFormatRoam   = "heat{:02}";
	const std::string configFormatRace   = "race{:02}";
	const std::string configPathMain     = "BartenderSettings/";
	const std::string configPathBasic    = configPathMain + "Basic/";
	const std::string configPathAdvanced = configPathMain + "Advanced/";


	


	// Special parsing functions --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	void ParseHeatParameter
	(
		ConfigParser::Parser&        parser,
		const std::string&           section,
		std::array<T, maxHeatLevel>& roamValues,
		std::array<T, maxHeatLevel>& raceValues,
		const std::optional<T>       defaultValue = {},
		const std::optional<T>       lowerBound   = {},
		const std::optional<T>       upperBound   = {}
	) {
		parser.ParseFormatParameter<T>(section, configFormatRoam, roamValues, defaultValue, lowerBound, upperBound);

		raceValues = roamValues;

		parser.ParseFormatParameter<T>(section, configFormatRace, raceValues, {}, lowerBound, upperBound);
	}



	template <typename T, typename U>
	void ParseHeatParameterPair
	(
		ConfigParser::Parser&        parser,
		const std::string&           section,
		std::array<T, maxHeatLevel>& firstRoamValues,
		std::array<U, maxHeatLevel>& secondRoamValues,
		std::array<T, maxHeatLevel>& firstRaceValues,
		std::array<U, maxHeatLevel>& secondRaceValues,
		const std::optional<T>       firstDefaultValue  = {},
		const std::optional<U>       secondDefaultValue = {},
		const std::optional<T>       firstLowerBound    = {},
		const std::optional<U>       secondLowerBound   = {},
		const std::optional<T>       firstUpperBound    = {},
		const std::optional<U>       secondUpperBound   = {}
	) {
		parser.ParseParameterTable
		(
			section,
			configFormatRoam,
			ConfigParser::FormatParameter<T, maxHeatLevel>(firstRoamValues,  firstDefaultValue,  firstLowerBound,  firstUpperBound),
			ConfigParser::FormatParameter<U, maxHeatLevel>(secondRoamValues, secondDefaultValue, secondLowerBound, secondUpperBound)
		);

		const std::array<bool, maxHeatLevel> isValids = parser.ParseParameterTable
		(
			section,
			configFormatRace,
			ConfigParser::FormatParameter<T, maxHeatLevel>(firstRaceValues,  {}, firstLowerBound,  firstUpperBound),
			ConfigParser::FormatParameter<U, maxHeatLevel>(secondRaceValues, {}, secondLowerBound, secondUpperBound)
		);

		for (size_t heatLevel = 1; heatLevel <= maxHeatLevel; heatLevel++)
		{
			if (not isValids[heatLevel - 1])
			{
				firstRaceValues[heatLevel - 1]  = firstRoamValues[heatLevel - 1];
				secondRaceValues[heatLevel - 1] = secondRoamValues[heatLevel - 1];
			}
		}
	}





	// Logging functions ----------------------------------------------------------------------------------------------------------------------------

	void OpenLog()
	{
		if (not logStream.get())
			logStream = std::make_unique<std::fstream>(logFile, std::ios::app);
	}



	template <typename T>
	void Print
	(
		std::fstream* const file, 
		const T             value
	) {
		*file << value;
	}



	template<>
	void Print<DWORD>
	(
		std::fstream* const file,
		const DWORD         value
	) {
		*file << std::format("{:08x}", value);
	}



	template<>
	void Print<float>
		(
			std::fstream* const file,
			const float         value
		) {
		*file << std::format("{:.3f}", value);
	}



	template <typename ...T>
	void Log(const T ...segments)
	{
		std::fstream* const file = logStream.get();
		if (not file) return;

		constexpr size_t numArgs = sizeof...(T);
		size_t           argID   = 0;

		([&]{Print<T>(file, segments); if (++argID < numArgs) *file << ' ';}(), ...);

		*file << std::endl;
	}



	template <typename ...T>
	void LogIndent(const T ...segments)
	{
		Log<std::string, T...>(logIndent, segments...);
	}



	template <typename ...T>
	void LogLongIndent(const T ...segments)
	{
		Log<std::string, T...>(logLongIndent, segments...);
	}
}