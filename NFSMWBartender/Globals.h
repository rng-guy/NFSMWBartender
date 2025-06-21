#pragma once

#include <Windows.h>
#include <fstream>
#include <string>

#undef min
#undef max

using address = DWORD;
using hash    = DWORD;
using key     = DWORD;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	constexpr bool   loggingEnabled = true;
	constexpr size_t maxHeatLevel   = 10;
	
	const std::string logFile            = "BartenderLog.txt";
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
	void Print<int>
	(
			std::fstream& file,
			const int&    value
	) {
		file << std::dec << value;
	}


	template<>
	void Print<address>
	(
		std::fstream&  file,
		const address& value
	) {
		file << std::hex << value;
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