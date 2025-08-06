#pragma once

#include <Windows.h>
#include <algorithm>
#include <optional>
#include <cstdint>
#include <fstream>
#include <memory>
#include <format>
#include <string>
#include <array>

#include "RandomNumbers.h"
#include "ConfigParser.h"

#undef min
#undef max



// Unscoped aliases
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





	// Custom hash function (if you can call it that) -----------------------------------------------------------------------------------------------

	struct IdentityHash 
	{
		size_t operator()(const uint32_t value) const
		{
			return value;
		}
	};





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	using HeatParameters = std::array<T, maxHeatLevel>;

	template <typename T>
	using FormatParameter = ConfigParser::FormatParameter<T, maxHeatLevel>;





	// HeatParametersPair struct --------------------------------------------------------------------------------------------------------------------

	template <typename T>
	struct HeatParametersPair
	{
		HeatParameters<T> roam = {};
		HeatParameters<T> race = {};


		HeatParameters<T>& operator ()(const bool forRaces)
		{
			return (forRaces) ? this->race : this->roam;
		}


		const HeatParameters<T>& operator ()(const bool forRaces) const
		{
			return (forRaces) ? this->race : this->roam;
		}


		T& operator ()
		(
			const bool   forRaces,
			const size_t heatLevel
		) {
			return (forRaces) ? this->race[heatLevel - 1] : this->roam[heatLevel - 1];
		}


		const T& operator ()
		(
			const bool   forRaces,
			const size_t heatLevel
		) 
			const 
		{
			return (forRaces) ? this->race[heatLevel - 1] : this->roam[heatLevel - 1];
		}
	};





	// Special parsing types and functions ----------------------------------------------------------------------------------------------------------

	template <typename T>
	struct ParsingSetup
	{
		HeatParametersPair<T>& pair;
		const std::optional<T> defaultValue = {};
		const std::optional<T> lowerBound   = {};
		const std::optional<T> upperBound   = {};
	};



	template <typename T>
	void ParseHeatParameters
	(
		ConfigParser::Parser& parser,
		const std::string&    section,
		ParsingSetup<T>&&     setup
	) {
		parser.ParseFormatParameter<T>
		(
			section,
			configFormatRoam,
			setup.pair.roam,
			setup.defaultValue,
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
	void ParseHeatParameters
	(
		ConfigParser::Parser&    parser,
		const std::string&       section,
		ParsingSetup<T>&&     ...columns
	) {
		parser.ParseParameterTable<T...>
		(
			section,
			configFormatRoam,
			FormatParameter<T>
			(
				columns.pair.roam, 
				columns.defaultValue, 
				columns.lowerBound, 
				columns.upperBound
			)...
		);

		const HeatParameters<bool> isValids = parser.ParseParameterTable<T...>
		(
			section,
			configFormatRace,
			FormatParameter<T>
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



	template <typename T>
	void CheckIntervalValues
	(
		HeatParametersPair<T>&       minValues,
		const HeatParametersPair<T>& maxValues
	) {
		for (const bool forRaces : {false, true})
		{
			HeatParameters<T>&       lowers = minValues(forRaces);
			const HeatParameters<T>& uppers = maxValues(forRaces);

			for (size_t heatLevel = 1; heatLevel <= maxHeatLevel; heatLevel++)
				lowers[heatLevel - 1] = std::min<T>(lowers[heatLevel - 1], uppers[heatLevel - 1]);
		}
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
	void Print<address>
	(
		std::fstream* const file,
		const address       value
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