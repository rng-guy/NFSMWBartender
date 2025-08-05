#pragma once

#include <Windows.h>
#include <algorithm>
#include <optional>
#include <cstdint>
#include <fstream>
#include <memory>
#include <format>
#include <string>

#include "RandomNumbers.h"
#include "ConfigParser.h"

#undef min
#undef max

using byte    = unsigned char;
using address = uint32_t;
using hash    = uint32_t;
using key     = uint32_t;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Pseudorandom number generator
	RandomNumbers::Generator prng;

	// Heat levels
	constexpr size_t maxHeatLevel = 10;
	constexpr float  maxHeat      = (float)maxHeatLevel;

	// Common function pointers
	hash (__cdecl* const GetStringHash)(const char*) = (hash (__cdecl*)(const char*))0x5CC240;
	hash (__thiscall* const GetCopType)(address)     = (hash (__thiscall*)(address))0x6880A0;
	
	// Configuration files
	const std::string configFormatRoam   = "heat{:02}";
	const std::string configFormatRace   = "race{:02}";
	const std::string configPathMain     = "BartenderSettings/";
	const std::string configPathBasic    = configPathMain + "Basic/";
	const std::string configPathAdvanced = configPathMain + "Advanced/";

	// Logging
	constexpr bool loggingEnabled = false;

	std::unique_ptr<std::fstream> logStream;

	const std::string logFile       = "BartenderLog.txt";
	const std::string logIndent     = "        ";
	const std::string logLongIndent = "              ";





	// Custom hash function (if you can call it that) --------------------------------------------------------------------------------------------------

	struct IdentityHash 
	{
		size_t operator()(const uint32_t value) const
		{
			return value;
		}
	};





	// Special parsing types and functions -------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct HeatParameter
	{
		std::array<T, maxHeatLevel>& roamValues;
		std::array<T, maxHeatLevel>& raceValues;
		const std::optional<T>       defaultValue = {};
		const std::optional<T>       lowerBound   = {};
		const std::optional<T>       upperBound   = {};
	};



	template <typename T>
	void ParseHeatParameter
	(
		ConfigParser::Parser& parser,
		const std::string&    section,
		HeatParameter<T>&&    parameter
	) {
		parser.ParseFormatParameter<T>
		(
			section, 
			configFormatRoam, 
			parameter.roamValues, 
			parameter.defaultValue, 
			parameter.lowerBound, 
			parameter.upperBound
		);

		parameter.raceValues = parameter.roamValues;

		parser.ParseFormatParameter<T>
		(
			section, 
			configFormatRace, 
			parameter.raceValues, 
			{}, 
			parameter.lowerBound, 
			parameter.upperBound
		);
	}



	template <typename ...T>
	void ParseHeatParameterTable
	(
		ConfigParser::Parser&    parser,
		const std::string&       section,
		HeatParameter<T>&&    ...columns
	) {
		parser.ParseParameterTable<T...>
		(
			section,
			configFormatRoam,
			ConfigParser::FormatParameter<T, maxHeatLevel>
			(
				columns.roamValues, 
				columns.defaultValue, 
				columns.lowerBound, 
				columns.upperBound
			)...
		);

		const std::array<bool, maxHeatLevel> isValids = parser.ParseParameterTable<T...>
		(
			section,
			configFormatRace,
			ConfigParser::FormatParameter<T, maxHeatLevel>
			(
				columns.raceValues, 
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
						columns.raceValues[heatLevel - 1] = columns.roamValues[heatLevel - 1];
				}
			}
		(), ...);
	}



	template <typename T>
	void CheckIntervalValues
	(
		std::array<T, maxHeatLevel>& minValues,
		std::array<T, maxHeatLevel>& maxValues
	) {
		for (size_t heatLevel = 1; heatLevel <= maxHeatLevel; heatLevel++)
			minValues[heatLevel - 1] = std::min<T>(minValues[heatLevel - 1], maxValues[heatLevel - 1]);
	}





	// Logging functions ----------------------------------------------------------------------------------------------------------------------------

	void OpenLog()
	{
		if (not logStream)
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
	void Print<uint32_t>
	(
		std::fstream* const file,
		const uint32_t      value
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
		if (not logStream) return;

		constexpr size_t    numArgs = sizeof...(T);
		std::fstream* const file    = logStream.get();
		size_t              argID   = 0;

		(
			[&]
			{
				Print<T>(file, segments); 
				if (++argID < numArgs) *file << ' ';
			}
		(), ...);

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