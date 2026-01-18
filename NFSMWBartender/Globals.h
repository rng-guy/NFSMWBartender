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

	// Game timer
	uint32_t pausedTicks = 0;

	// Common objects
	RandomNumbers::Generator prng;
	BasicLogger  ::Logger    logger;

	// Player state
	address playerPerpVehicle    = 0x0;
	bool    playerHeatLevelKnown = false;

	// Logging flag
	constexpr bool loggingEnabled = false;

	// Common function pointers
	const auto GetVaultKey  = reinterpret_cast<vault  (__cdecl*)(const char*)>(0x5CC240);
	const auto GetBinaryKey = reinterpret_cast<binary (__cdecl*)(const char*)>(0x460BF0);

	const auto GetVehicleType = reinterpret_cast<vault       (__thiscall*)(address)>(0x6880A0);
	const auto GetVehicleName = reinterpret_cast<const char* (__thiscall*)(address)>(0x688090);

	const auto IsPlayerPursuit     = reinterpret_cast<bool (__thiscall*)(address)>(0x40AD80);
	const auto IsVehicleDestroyed  = reinterpret_cast<bool (__thiscall*)(address)>(0x688170);
	const auto ClearSupportRequest = reinterpret_cast<void (__thiscall*)(address)>(0x42BCF0);

	// Common data pointers
	const float&    simulationTime = *reinterpret_cast<float*>   (0x9885D8);
	const address&  copManager     = *reinterpret_cast<address*> (0x989098);
	const uint32_t& gameTicks      = *reinterpret_cast<uint32_t*>(0x925B14);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	float __fastcall GetGameTime(const bool unpaused)
	{
		const float    ticksToTime = *reinterpret_cast<float*>(0x890984);
		const uint32_t ticks       = (unpaused) ? (gameTicks - pausedTicks) : gameTicks;

		return ticksToTime * static_cast<float>(ticks);
	}



	address GetFromVault
	(
		const vault  rootKey,
		const vault  nodeKey,
		const vault  attributeKey   = 0x0,
		const size_t attributeIndex = 0
	) {
		const auto GetVaultNode      = reinterpret_cast<address (__cdecl*)   (vault, vault)>          (0x455FD0);
		const auto GetVaultAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454190);

		const address node = GetVaultNode(rootKey, nodeKey);
		return (node and attributeKey) ? GetVaultAttribute(node, attributeKey, attributeIndex) : node;
	}



	address GetFromPursuitlevel
	(
		const address pursuit,
		const vault   attributeKey,
		const size_t  attributeIndex = 0
	){
		const auto GetPursuitNode      = reinterpret_cast<address (__thiscall*)(address)>               (0x418E90);
		const auto GetPursuitAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454810);

		const address node = GetPursuitNode(pursuit);
		return (node) ? GetPursuitAttribute(node, attributeKey, attributeIndex) : node;
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