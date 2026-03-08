#pragma once

#include <cstdint>

#include "MemoryTools.h"
#include "BasicLogger.h"
#include "RandomNumbers.h"

#undef min
#undef max



// Unscoped aliases
using vault  = uint32_t;
using binary = uint32_t;

using byte = MemoryTools::byte;
using word = MemoryTools::word;

using address = MemoryTools::address;



namespace Globals
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Feature flags
	bool basicSetEnabled    = false;
	bool advancedSetEnabled = false;

	// Game timer
	uint32_t pausedTicks = 0;

	// Random-number generator
	RandomNumbers::Generator prng;

	// Player state
	address playerPerpVehicle    = 0x0;
	bool    playerHeatLevelKnown = false;

	// Logging
	constexpr bool loggingEnabled = false;
	BasicLogger::Logger logger(9, 15, 17);

	// Common function pointers
	constinit const auto GetVaultKey  = reinterpret_cast<vault  (__cdecl*)(const char*)>(0x5CC240);
	constinit const auto GetBinaryKey = reinterpret_cast<binary (__cdecl*)(const char*)>(0x460BF0);

	constinit const auto GetVehicleType = reinterpret_cast<vault       (__thiscall*)(address)>(0x6880A0);
	constinit const auto GetVehicleName = reinterpret_cast<const char* (__thiscall*)(address)>(0x688090);

	constinit const auto IsPlayerPursuit     = reinterpret_cast<bool (__thiscall*)(address)>(0x40AD80);
	constinit const auto IsVehicleDestroyed  = reinterpret_cast<bool (__thiscall*)(address)>(0x688170);
	constinit const auto ClearSupportRequest = reinterpret_cast<void (__thiscall*)(address)>(0x42BCF0);

	// Common data pointers
	const volatile float&    simulationTime = *reinterpret_cast<volatile float*>   (0x9885D8);
	const volatile address&  copManager     = *reinterpret_cast<volatile address*> (0x90D5F4);
	const volatile uint32_t& gameTicks      = *reinterpret_cast<volatile uint32_t*>(0x925B14);





	// State functions ------------------------------------------------------------------------------------------------------------------------------

	bool IsInCooldownMode(const address pursuit)
	{
		return (*reinterpret_cast<volatile int*>(pursuit + 0x218) == 2); // pursuitStatus
	}



	float __fastcall GetGameTime(const bool unpaused)
	{
		const float    ticksToTime = *reinterpret_cast<volatile float*>(0x890984);
		const uint32_t ticks       = (unpaused) ? (gameTicks - pausedTicks) : gameTicks;

		return ticksToTime * static_cast<float>(ticks);
	}





	// Vault functions ------------------------------------------------------------------------------------------------------------------------------

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
	) {
		const auto GetPursuitNode      = reinterpret_cast<address (__thiscall*)(address)>               (0x418E90);
		const auto GetPursuitAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454810);

		const address node = GetPursuitNode(pursuit);
		return (node) ? GetPursuitAttribute(node, attributeKey, attributeIndex) : node;
	}

	



	// Vehicle-type functions -----------------------------------------------------------------------------------------------------------------------

	vault GetVehicleTypeClass(const vault type)
	{
		const address attribute = GetFromVault(0x4A97EC8F, type, 0x0EF6DDF2); // fetches "CLASS" from "pvehicle"
		return (attribute) ? *reinterpret_cast<volatile vault*>(attribute + 0x8) : attribute;
	}



	bool DoesVehicleTypeExist(const vault type)
	{
		return GetVehicleTypeClass(type);
	}


	bool IsVehicleTypeCar(const vault type)
	{
		switch (GetVehicleTypeClass(type))
		{
		case 0x336FCACF: // CAR
		case 0x89E87652: // TRACTOR
			return true;
		}

		return false;
	}


	bool IsVehicleTypeChopper(const vault type)
	{
		return (GetVehicleTypeClass(type) == 0xB80933AA); // CHOPPER
	}





	// Vehicle-object functions ---------------------------------------------------------------------------------------------------------------------

	address GetAIVehicle(const address vehicle)
	{
		return *reinterpret_cast<volatile address*>(vehicle + 0x54);
	}



	address GetAIVehiclePursuit(const address copVehicle)
	{
		const address copAIVehicle = GetAIVehicle(copVehicle);
		return (copAIVehicle) ? (copAIVehicle - 0x4C + 0x758) : copAIVehicle;
	}



	bool EndSupportGoal(const address copVehicle)
	{
		const address copAIVehicle = GetAIVehicle(copVehicle);
		if (not copAIVehicle) return false; // should never happen

		const address copAIVehiclePursuit = GetAIVehiclePursuit(copVehicle);
		if (not copAIVehiclePursuit) return false; // should never happen

		const auto SetSupportGoal = reinterpret_cast<void (__thiscall*)(address, vault)>       (0x409850);
		const auto SetVehicleGoal = reinterpret_cast<void (__thiscall*)(address, const vault*)>(0x422480);

		constexpr vault pursuitGoal = 0x492916A2; // "AIGoalPursuit"

		SetSupportGoal(copAIVehiclePursuit, 0x0);
		SetVehicleGoal(copAIVehicle - 0x4C, &pursuitGoal);

		return true;
	}
}