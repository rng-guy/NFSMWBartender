#pragma once

#include <Windows.h>
#include <fstream>
#include <format>
#include <string>

#undef min
#undef max

using address = DWORD;
using hash    = DWORD;
using key     = DWORD;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	constexpr bool   loggingEnabled = false;
	constexpr size_t maxHeatLevel   = 10;
	
	const std::string logFile            = "BartenderLog.txt";
	const std::string logIndent          = "        ";
	const std::string configFormat       = "heat{:02}";
	const std::string configMainPath     = "BartenderSettings/";
	const std::string configBasicPath    = configMainPath + "Basic/";
	const std::string configAdvancedPath = configMainPath + "Advanced/";





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	void Print
	(
		std::fstream& file, 
		const T&      value
	) {
		file << value;
	}



	template<>
	void Print<DWORD>
	(
		std::fstream&  file,
		const DWORD& value
	) {
		file << std::format("{:08x}", value);
	}



	template <typename ...T>
	void Log(T ...segments)
	{
		constexpr size_t numArgs = sizeof...(T);

		std::fstream file;
		file.open(logFile, std::ios::app);

		size_t argID = 0;
		([&] {Print<T>(file, segments); if (++argID < numArgs) file << " ";}(), ...);

		file << std::endl;
		file.close();
	}
}