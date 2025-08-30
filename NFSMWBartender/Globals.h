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

	// Common function pointers
	vault       (__cdecl*    const GetVaultKey)   (const char*) = (vault  (__cdecl*)        (const char*))0x5CC240;
	vault       (__thiscall* const GetVehicleType)(address)     = (vault  (__thiscall*)     (address))    0x6880A0;
	const char* (__thiscall* const GetVehicleName)(address)     = (const char* (__thiscall*)(address))    0x688090;

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
		static address (__cdecl*    const GetVaultNode)     (vault, vault)           = (address (__cdecl*)   (vault, vault))          0x455FD0;
		static address (__thiscall* const GetVaultAttribute)(address, vault, size_t) = (address (__thiscall*)(address, vault, size_t))0x454190;

		const address node = GetVaultNode(rootKey, nodeKey);
		return (node and attributeKey) ? GetVaultAttribute(node, attributeKey, attributeIndex) : node;
	}



	bool VehicleExists(const vault vehicleType)
	{
		return GetFromVault(0x4A97EC8F, vehicleType); // fetches vehicle node from "pvehicle"
	}



	bool VehicleClassMatches
	(
		const vault vehicleType,
		const bool  forHelicopter
	) {
		const address attribute = GetFromVault(0x4A97EC8F, vehicleType, 0x0EF6DDF2); // fetches "CLASS" from "pvehicle"

		if (not attribute) return false;

		const vault vehicleClass = *((vault*)(attribute + 0x8));
		const bool  isHelicopter = (vehicleClass == 0xB80933AA); // checks if "CHOPPER"

		return (isHelicopter == forHelicopter);
	}
}