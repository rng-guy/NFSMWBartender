#pragma once

#include <cstdint>

#include "MemoryTools.h"
#include "BasicLogger.h"
#include "RandomNumbers.h"



// In debug releases, Visual Studio forces an unconditional dynamic allocation for compatible types.
// This makes all dynamically allocating types (e.g. std::vector, std::string) constinit-incompatible.
#ifdef _DEBUG
#define RELEASE_CONSTINIT

#else
#define RELEASE_CONSTINIT constinit

#endif



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

	// Logging (e.g. for debugging)
	constexpr bool loggingEnabled = false;
	BasicLogger::Logger<9, 15, 17> logger;

	// Hackjob floating-point correction coefficient
	constexpr float floatScale = 1.f + static_cast<float>(1e-6);

	// Common function pointers
	const auto GetVaultKey  = reinterpret_cast<vault  (__cdecl*)(const char*)>(0x5CC240);
	const auto GetBinaryKey = reinterpret_cast<binary (__cdecl*)(const char*)>(0x460BF0);

	const auto GetVehicleType = reinterpret_cast<vault       (__thiscall*)(address)>(0x6880A0);
	const auto GetVehicleName = reinterpret_cast<const char* (__thiscall*)(address)>(0x688090);

	const auto IsPlayerPursuit     = reinterpret_cast<bool (__thiscall*)(address)>(0x40AD80);
	const auto IsVehicleDestroyed  = reinterpret_cast<bool (__thiscall*)(address)>(0x688170);
	const auto ClearSupportRequest = reinterpret_cast<void (__thiscall*)(address)>(0x42BCF0);

	// Common data pointers
	const volatile float&    simulationTime = *reinterpret_cast<volatile float*>   (0x9885D8); // seconds
	const volatile address&  copManager     = *reinterpret_cast<volatile address*> (0x90D5F4);
	const volatile uint32_t& gameTicks      = *reinterpret_cast<volatile uint32_t*>(0x925B14);
	const volatile float&    ticksToTime    = *reinterpret_cast<volatile float*>   (0x890984); // seconds / tick





	// State functions ------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool IsInCooldownMode(const address pursuit)
	{
		return (*reinterpret_cast<volatile int*>(pursuit + 0x218) == 2); // pursuit status
	}



	[[nodiscard]] float GetTotalGameTime()
	{
		return ticksToTime * static_cast<float>(gameTicks);
	}


	[[nodiscard]] float GetUnpausedGameTime()
	{
		return ticksToTime * static_cast<float>(gameTicks - pausedTicks);
	}





	// Vault functions ------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] address GetFromVault
	(
		const vault  rootKey,
		const vault  nodeKey,
		const vault  attributeKey   = 0x0,
		const size_t attributeIndex = 0
	) {
		const auto GetVaultNode      = reinterpret_cast<address (__cdecl*)   (vault,   vault)>        (0x455FD0);
		const auto GetVaultAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454190);

		const address node = GetVaultNode(rootKey, nodeKey);

		return (node and attributeKey) ? GetVaultAttribute(node, attributeKey, attributeIndex) : node;
	}



	[[nodiscard]] address GetFromPursuitlevel
	(
		const address pursuit,
		const vault   attributeKey,
		const size_t  attributeIndex = 0
	) {
		const auto GetPursuitNode      = reinterpret_cast<address (__thiscall*)(address)>               (0x418E90);
		const auto GetPursuitAttribute = reinterpret_cast<address (__thiscall*)(address, vault, size_t)>(0x454810);

		const address node = GetPursuitNode(pursuit);

		return (node) ? GetPursuitAttribute(node, attributeKey, attributeIndex) : 0x0;
	}

	



	// Vehicle-type functions -----------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] vault GetVehicleTypeClass(const vault type)
	{
		const address attribute = GetFromVault(0x4A97EC8F, type, 0x0EF6DDF2); // fetches "CLASS" from "pvehicle"
		return (attribute) ? *reinterpret_cast<volatile vault*>(attribute + 0x8) : 0x0;
	}



	[[nodiscard]] bool DoesVehicleTypeExist(const vault type)
	{
		return GetVehicleTypeClass(type);
	}


	[[nodiscard]] bool IsVehicleTypeCar(const vault type)
	{
		switch (GetVehicleTypeClass(type))
		{
		case 0x336FCACF: // CAR
		case 0x89E87652: // TRACTOR
			return true;
		}

		return false;
	}


	[[nodiscard]] bool IsVehicleTypeChopper(const vault type)
	{
		return (GetVehicleTypeClass(type) == 0xB80933AA); // CHOPPER
	}





	// Vehicle-object functions ---------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] address GetPlayerVehicle()
	{
		return (playerPerpVehicle) ? *reinterpret_cast<volatile address*>(playerPerpVehicle - 0x758 + 0x4C - 0x4) : 0x0;
	}


	[[nodiscard]] address GetAIVehicle(const address vehicle)
	{
		return (vehicle) ? *reinterpret_cast<volatile address*>(vehicle + 0x54) : 0x0;
	}


	[[nodiscard]] address GetAIVehiclePursuit(const address copVehicle)
	{
		const address copAIVehicle = GetAIVehicle(copVehicle);
		return (copAIVehicle) ? (copAIVehicle - 0x4C + 0x758) : 0x0;
	}



	bool EndSupportGoal(const address copVehicle)
	{
		const address copAIVehicle = GetAIVehicle(copVehicle);
		if (not copAIVehicle) return false; // should never happen

		const address copAIVehiclePursuit = GetAIVehiclePursuit(copVehicle);
		if (not copAIVehiclePursuit) return false; // should never happen

		const auto SetSupportGoal = reinterpret_cast<void (__thiscall*)(address, vault)>       (0x409850);
		const auto SetVehicleGoal = reinterpret_cast<void (__thiscall*)(address, const vault*)>(0x422480);

		static constexpr vault pursuitGoal = 0x492916A2; // "AIGoalPursuit"
	
		SetSupportGoal(copAIVehiclePursuit, 0x0);
		SetVehicleGoal(copAIVehicle - 0x4C, &pursuitGoal);

		return true;
	}
}