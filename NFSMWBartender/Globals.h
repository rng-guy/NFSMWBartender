#pragma once

#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#include "BasicLogger.h"
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

	BasicLogger::Logger logger;





	// Custom hash function and (scoped) aliases ----------------------------------------------------------------------------------------------------

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
}