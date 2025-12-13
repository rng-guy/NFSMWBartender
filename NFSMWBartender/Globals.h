#pragma once

#include <concepts>

#include "BasicLogger.h"
#include "RandomNumbers.h"

#undef min
#undef max



// Unscoped aliases
using byte    = unsigned char;
using address = uintptr_t;
using binary  = uint32_t;
using vault   = uint32_t;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Feature flags
	bool basicSetEnabled    = false;
	bool advancedSetEnabled = false;

	// Common objects
	RandomNumbers::Generator prng;
	BasicLogger::Logger      logger;

	// Player state
	address playerPerpVehicle    = 0x0;
	bool    playerHeatLevelKnown = false;

	// Logging flag
	constexpr bool loggingEnabled = true;

	// Common function pointers
	const auto GetVaultKey  = reinterpret_cast<vault  (__cdecl*)(const char*)>(0x5CC240);
	const auto GetBinaryKey = reinterpret_cast<binary (__cdecl*)(const char*)>(0x460BF0);

	const auto GetVehicleType = reinterpret_cast<vault       (__thiscall*)(address)>(0x6880A0);
	const auto GetVehicleName = reinterpret_cast<const char* (__thiscall*)(address)>(0x688090);

	const auto IsPlayerPursuit    = reinterpret_cast<bool (__thiscall*)(address)>(0x40AD80);
	const auto IsVehicleDestroyed = reinterpret_cast<bool (__thiscall*)(address)>(0x688170);

	// Common data pointers
	const float& simulationTime = *reinterpret_cast<float*>(0x9885D8);

	



	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	address GetFromVault
	(
		const vault  rootKey,
		const vault  nodeKey,
		const vault  attributeKey   = 0x0,
		const size_t attributeIndex = 0
	) {
		static const auto GetVaultNode      = reinterpret_cast<address (__cdecl*)   (vault, vault)>          (0x455FD0);
		static const auto GetVaultAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454190);

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

		const vault vehicleClass = *reinterpret_cast<vault*>(attribute + 0x8);
		const bool  isHelicopter = (vehicleClass == 0xB80933AA); // checks if "CHOPPER"

		return (isHelicopter == (targetClass == Class::CHOPPER));
	}
}