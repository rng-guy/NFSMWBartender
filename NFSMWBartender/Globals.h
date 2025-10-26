#pragma once

#include "BasicLogger.h"
#include "RandomNumbers.h"

#undef min
#undef max



// Unscoped aliases
using byte    = unsigned char;
using address = uint32_t;
using binary  = uint32_t;
using vault   = uint32_t;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Feature flags
	bool basicSetEnabled    = false;
	bool advancedSetEnabled = false;

	// Pseudorandom number generator
	RandomNumbers::Generator prng;

	// Function pointers
	const auto IsPlayerPursuit = (bool   (__thiscall*)(address))    0x40AD80;
	const auto GetBinaryKey    = (binary (__cdecl*)   (const char*))0x460BF0;
	const auto GetVaultKey     = (vault  (__cdecl*)   (const char*))0x5CC240;

	// Logging
	constexpr bool loggingEnabled = false;

	BasicLogger::Logger logger;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	address GetFromVault
	(
		const vault  rootKey,
		const vault  nodeKey,
		const vault  attributeKey   = 0x0,
		const size_t attributeIndex = 0
	) {
		static const auto GetVaultNode      = (address (__cdecl*)   (vault, vault))          0x455FD0;
		static const auto GetVaultAttribute = (address (__thiscall*)(address, vault, size_t))0x454190;

		const address node = GetVaultNode(rootKey, nodeKey);
		return (node and attributeKey) ? GetVaultAttribute(node, attributeKey, attributeIndex) : node;
	}



	enum class Class
	{
		ANY,
		CAR,
		CHOPPER
	};

	bool VehicleClassMatches
	(
		const vault vehicleType,
		const Class targetClass
	) {
		const address attribute = GetFromVault(0x4A97EC8F, vehicleType, 0x0EF6DDF2); // fetches "CLASS" from "pvehicle"
		if ((not attribute) or (targetClass == Class::ANY)) return attribute;

		const vault vehicleClass = *((vault*)(attribute + 0x8));
		const bool  isHelicopter = (vehicleClass == 0xB80933AA); // checks if "CHOPPER"

		return (isHelicopter == (targetClass == Class::CHOPPER));
	}
}