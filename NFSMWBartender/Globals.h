#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <fstream>
#include <memory>
#include <format>
#include <string>

#include "RandomNumbers.h"

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

	// Common function pointers
	hash (__cdecl* const GetStringHash)(const char*) = (hash (__cdecl*)(const char*))0x5CC240;
	hash (__thiscall* const GetCopType)(address)     = (hash (__thiscall*)(address))0x6880A0;

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

	

	template <typename T>
	using AddressMap = std::unordered_map<address, T, IdentityHash>;

	template <typename T>
	using HashMap = std::unordered_map<hash, T, IdentityHash>;

	using AddressSet = std::unordered_set<address, IdentityHash>;
	using HashSet    = std::unordered_set<hash,    IdentityHash>;




	
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