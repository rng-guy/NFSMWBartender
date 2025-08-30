#pragma once

#include <unordered_set>
#include <unordered_map>

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





	// Custom hash function and (scoped) aliases ----------------------------------------------------------------------------------------------------

	template <typename K>
	struct IdentityHash 
	{
		size_t operator()(const K value) const
		{
			return value;
		}
	};

	

	template <typename K>
	using IdentitySet = std::unordered_set<K, IdentityHash<K>>;

	using AddressSet = IdentitySet<address>;
	using VaultSet   = IdentitySet<vault>;



	template <typename K, typename V>
	using IdentityMap = std::unordered_map<K, V, IdentityHash<K>>;

	template <typename V>
	using AddressMap = IdentityMap<address, V>;

	template <typename V>
	using VaultMap = IdentityMap<vault, V>;





	// Parsing and validation functions -------------------------------------------------------------------------------------------------------------

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



	template <typename T, typename K, typename V>
	void ValidateVaultMap
	(
		const char* const mapName,
		VaultMap<T>&      map,
		T&                defaultValue,
		const K&          keyPredicate,
		const V&          valuePredicate
	) {
		// Extract "default" key if provided (and valid)
		const auto pair    = map.extract(GetVaultKey("default"));
		const bool isValid = ((not pair.empty()) and valuePredicate(pair.mapped()));

		if (isValid)
			defaultValue = pair.mapped();

		if constexpr (loggingEnabled)
			logger.LogLongIndent((isValid) ? "Valid default value:" : "No valid default value, using", defaultValue);
		
		// Remove any invalid pairs
		auto   iterator   = map.begin();
		size_t numRemoved = 0;

		while (iterator != map.end())
		{
			if (not (keyPredicate(iterator->first) and valuePredicate(iterator->second)))
			{
				if constexpr (loggingEnabled)
				{
					if (numRemoved == 0)
						logger.LogLongIndent(mapName, "map");

					logger.LogLongIndent("  -", iterator->first, iterator->second);
				}

				iterator = map.erase(iterator);

				numRemoved++;
			}
			else iterator++;
		}

		if constexpr (loggingEnabled)
		{
			if (numRemoved > 0)
				logger.LogLongIndent("  pairs left:", (int)map.size());

			else
				logger.LogLongIndent(mapName, "pairs:", (int)map.size());
		}
	}
}