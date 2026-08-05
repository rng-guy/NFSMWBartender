#pragma once

#include <cstdint>
#include <string_view>

#include "MemoryTools.h"
#include "BasicLogger.h"
#include "RandomNumbers.h"



// In debug builds, Visual Studio forces an unconditional dynamic allocation for each suitable type.
// This makes dynamic containers (e.g. std::vector, std::string) constinit-incompatible, even if empty.
#ifndef _DEBUG
#define RELEASE_CONSTINIT constinit

#else
#define RELEASE_CONSTINIT

#endif



// Unscoped aliases
using vault  = uint32_t;
using binary = uint32_t;

using byte = MemoryTools::byte;
using word = MemoryTools::word;

using address = MemoryTools::address;

// Unscoped functions
using MemoryTools::AsPointer;
using MemoryTools::AsReference;
using MemoryTools::AsFunction;

// Unscoped types
using BasicLogger::BinFormat;
using BasicLogger::DecFormat;
using BasicLogger::HexFormat;



namespace Globals
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	// Feature flags
	bool basicSetEnabled    = false;
	bool advancedSetEnabled = false;

	// Game timer
	uint32_t numPausedTicks = 0;

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
	const auto IsPlayerPursuit     = AsFunction<bool __thiscall (address)>(0x40AD80); // for Pursuit
	const auto IsVehicleDestroyed  = AsFunction<bool __thiscall (address)>(0x688170); // for PVehicle
	const auto ClearSupportRequest = AsFunction<void __thiscall (address)>(0x42BCF0); // for Pursuit

	const auto GetVehicleType = AsFunction<vault       __thiscall (address)>(0x6880A0); // for PVehicle
	const auto GetVehicleName = AsFunction<const char* __thiscall (address)>(0x688090); // for PVehicle

	// Common data pointers
	const float&    simulationTime = AsReference<float>   (0x9885D8); // seconds
	const address&  copManager     = AsReference<address> (0x90D5F4);
	const uint32_t& numGameTicks   = AsReference<uint32_t>(0x925B14);
	const float&    ticksToTime    = AsReference<float>   (0x890984); // seconds / tick





	// Hash functions -------------------------------------------------------------------------------------------------------------------------------
	
	[[nodiscard]] constexpr vault GetVaultHash(std::string_view input)
	{
		if (input.empty()) return 0x0;

		vault a = 0x9E3779B9; // golden ratio
		vault b = a;
		vault c = 0xABCDEF00; // MW-specific seed

		const auto Shift = [&input](const size_t i, const size_t n) -> vault
		{
			// force zero-extension first to avoid underflow in second cast
			return (static_cast<vault>(static_cast<unsigned char>(input[i])) << n);
		};

		const auto MixValues = [&a, &b, &c]() -> void
		{
			a -= b; a -= c; a ^= (c >> 13);
			b -= c; b -= a; b ^= (a <<  8);
			c -= a; c -= b; c ^= (b >> 13);
			a -= b; a -= c; a ^= (c >> 12);
			b -= c; b -= a; b ^= (a << 16);
			c -= a; c -= b; c ^= (b >>  5);
			a -= b; a -= c; a ^= (c >>  3);
			b -= c; b -= a; b ^= (a << 10);
			c -= a; c -= b; c ^= (b >> 15);
		};

		const size_t size = input.size();

		while (input.size() >= 12)
		{
			a += Shift(0, 0) + Shift(1, 8) + Shift( 2, 16) + Shift( 3, 24);
			b += Shift(4, 0) + Shift(5, 8) + Shift( 6, 16) + Shift( 7, 24);
			c += Shift(8, 0) + Shift(9, 8) + Shift(10, 16) + Shift(11, 24);

			MixValues();

			input.remove_prefix(12);
		}

		switch (input.size())
		{
			case 11: c += Shift(10, 24); [[fallthrough]];
			case 10: c += Shift( 9, 16); [[fallthrough]];
			case  9: c += Shift( 8,  8); [[fallthrough]];
			case  8: b += Shift( 7, 24); [[fallthrough]];
			case  7: b += Shift( 6, 16); [[fallthrough]];
			case  6: b += Shift( 5,  8); [[fallthrough]];
			case  5: b += Shift( 4,  0); [[fallthrough]];
			case  4: a += Shift( 3, 24); [[fallthrough]];
			case  3: a += Shift( 2, 16); [[fallthrough]];
			case  2: a += Shift( 1,  8); [[fallthrough]];
			case  1: a += Shift( 0,  0); [[fallthrough]];
			case  0: c += size;
		}

		MixValues();

		return c;
	}


	[[nodiscard]] consteval vault operator""_vlt
	(
		const char* const string,
		const size_t      length
	) {
		return GetVaultHash({string, length});
	}



	[[nodiscard]] constexpr binary GetBinaryHash(const std::string_view input)
	{
		if (input.empty()) return 0x0;

		binary hash = 0xFFFFFFFF; // MW-specific seed

		for (const char ch : input)
			hash += (hash << 5) + static_cast<unsigned char>(ch);

		return hash;
	}


	[[nodiscard]] consteval binary operator""_bin
	(
		const char* const string,
		const size_t      length
	) {
		return GetBinaryHash({string, length});
	}





	// Pursuit iteration ----------------------------------------------------------------------------------------------------------------------------

	class PursuitList
	{
	private:

		// Linked-list iterator for AICopManager
		class EntryIterator
		{
		private:

			address current;


		public:

			explicit EntryIterator(const address entry) : current(entry) {}


			address operator*() const
			{
				return AsReference<address>(this->current + 0x8);
			}


			EntryIterator& operator++()
			{
				this->current = AsReference<address>(this->current);

				return *this;
			}


			bool operator==(const EntryIterator&) const = default;
		};


	private:

		address first    = 0x0;
		address sentinel = 0x0;


	public:

		PursuitList()
		{
			if (not copManager) return;

			this->sentinel = AsReference<address>(copManager + 0x128);
			this->first    = AsReference<address>(sentinel);
		}


		EntryIterator begin() const {return EntryIterator(this->first);}
		EntryIterator end  () const {return EntryIterator(this->sentinel);}
	};





	// Timer functions ------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] float GetTotalGameTime()
	{
		return ticksToTime * static_cast<float>(numGameTicks);
	}


	[[nodiscard]] float GetUnpausedGameTime()
	{
		return ticksToTime * static_cast<float>(numGameTicks - numPausedTicks);
	}





	// Vault functions ------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] address GetFromVault
	(
		const vault  rootKey,
		const vault  nodeKey,
		const vault  attributeKey   = ""_vlt,
		const size_t attributeIndex = 0
	) {
		const auto GetVaultNode          = AsFunction<address __cdecl    (vault,   vault)>        (0x455FD0);
		const auto GetVaultNodeAttribute = AsFunction<address __thiscall (address, vault, size_t)>(0x454190);

		const address node = GetVaultNode(rootKey, nodeKey);
		if (not node) return 0x0; // unknown node

		return (attributeKey != ""_vlt) ? GetVaultNodeAttribute(node, attributeKey, attributeIndex) : node;
	}



	[[nodiscard]] address GetFromPursuitlevel
	(
		const address pursuit,
		const vault   attributeKey,
		const size_t  attributeIndex = 0
	) {
		const auto GetPursuitNode          = AsFunction<address __thiscall (address)>               (0x418E90);
		const auto GetPursuitNodeAttribute = AsFunction<address __thiscall (address, vault, size_t)>(0x454810);

		const address node = GetPursuitNode(pursuit);
		if (not node) return 0x0; // unknown node

		return GetPursuitNodeAttribute(node, attributeKey, attributeIndex);
	}

	



	// Vehicle-type functions -----------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] vault GetClassOfVehicleType(const vault type)
	{
		const address attribute = GetFromVault("pvehicle"_vlt, type, "CLASS"_vlt);
		return (attribute) ? AsReference<vault>(attribute + 0x8) : ""_vlt;
	}



	[[nodiscard]] bool DoesVehicleTypeExist(const vault type)
	{
		return (GetClassOfVehicleType(type) != ""_vlt);
	}


	[[nodiscard]] bool IsVehicleTypeCar(const vault type)
	{
		const vault typeClass = GetClassOfVehicleType(type);

		switch (typeClass)
		{
		case     "CAR"_vlt:
		case "TRACTOR"_vlt:
			return true;
		}

		return false;
	}


	[[nodiscard]] bool IsVehicleTypeChopper(const vault type)
	{
		return (GetClassOfVehicleType(type) == "CHOPPER"_vlt);
	}





	// Vehicle-object functions ---------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] address GetAIVehicleOfVehicle(const address vehicle)
	{
		if (not vehicle) return 0x0;
		return AsReference<address>(vehicle + 0x54);
	}
	

	[[nodiscard]] address GetAIVehiclePursuitOfVehicle(const address copVehicle)
	{
		const address copAIVehicle = GetAIVehicleOfVehicle(copVehicle);
		if (not copAIVehicle) return 0x0;

		return copAIVehicle + (0x758 - 0x4C);
	}



	[[nodiscard]] address GetAIVehicleOfPerpVehicle(const address perpVehicle)
	{
		if (not perpVehicle) return 0x0;
		return perpVehicle - (0x758 - 0x4C);
	}
	

	[[nodiscard]] address GetVehicleOfPerpVehicle(const address perpVehicle)
	{
		if (not perpVehicle) return 0x0;
		return AsReference<address>(GetAIVehicleOfPerpVehicle(perpVehicle) - 0x4);
	}


	[[nodiscard]] address GetPursuitOfPerpVehicle(const address perpVehicle)
	{
		if (not perpVehicle) return 0x0;
		return AsReference<address>(GetAIVehicleOfPerpVehicle(perpVehicle) + 0x70);
	}



	bool EndSupportGoalOfVehicle(const address copVehicle)
	{
		const address copAIVehicle = GetAIVehicleOfVehicle(copVehicle);
		if (not copAIVehicle) return false;

		const address copAIVehiclePursuit = GetAIVehiclePursuitOfVehicle(copVehicle);
		if (not copAIVehiclePursuit) return false; // should never happen

		const auto SetSupportGoal = AsFunction<void __thiscall (address, vault)>       (0x409850);
		const auto SetVehicleGoal = AsFunction<void __thiscall (address, const vault&)>(0x422480);

		SetSupportGoal(copAIVehiclePursuit, ""_vlt); // empty goal
		SetVehicleGoal(copAIVehicle - 0x4C, "AIGoalPursuit"_vlt);

		return true;
	}





	// Pursuit functions ----------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] address GetPhysicsObjectOfPursuitTarget(const address pursuit)
	{
		if (not pursuit) return 0x0;

		const address pursuitTarget = AsReference<address>(pursuit + 0x74);
		if (not pursuitTarget) return 0x0; // should never happen

		return AsReference<address>(pursuitTarget + 0x1C);
	}



	[[nodiscard]] address GetPerpVehicleOfPursuit(const address pursuit)
	{
		if (not pursuit) return 0x0;

		const address pursuitTarget = AsReference<address>(pursuit + 0x74);
		if (not pursuitTarget) return 0x0; // should never happen

		address perpVehicle = 0x0;

		const auto FindPerpVehicle = AsFunction<bool __thiscall (address, address&)>(0x40E200);
		FindPerpVehicle(pursuitTarget, perpVehicle); // doesn't write garbage if search fails
		
		return perpVehicle;
	}


	[[nodiscard]] address GetLocalPlayerOfPursuit(const address pursuit)
	{
		if (not pursuit) return 0x0;

		const address physicsObject = GetPhysicsObjectOfPursuitTarget(pursuit);
		if (not physicsObject) return 0x0; // should never happen

		return AsReference<address>(physicsObject + 0x58);
	}



	[[nodiscard]] bool IsPursuitInCooldownMode(const address pursuit)
	{
		if (not pursuit) return false;

		const int pursuitStatus = AsReference<int>(pursuit + 0x218);
		return (pursuitStatus == 2); // "COOLDOWN" mode
	}





	// Player functions -----------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool IsLocalPlayerInPursuit(const address localPlayer)
	{
		if (not localPlayer) return false;

		for (const address pursuit : PursuitList())
			if (localPlayer == GetLocalPlayerOfPursuit(pursuit)) return true;

		return false;
	}
}



// Unscoped operators
using Globals::operator""_vlt;
using Globals::operator""_bin;