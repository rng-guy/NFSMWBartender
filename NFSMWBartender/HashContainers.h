#pragma once

#include <unordered_set>
#include <unordered_map>

#include "Globals.h"



namespace HashContainers
{

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

	template <typename T, typename K, typename V>
	void ValidateVaultMap
	(
		const char* const mapName,
		VaultMap<T>& map,
		T& defaultValue,
		const K& keyPredicate,
		const V& valuePredicate
	) {
		// Extract "default" key if provided (and valid)
		const auto pair    = map.extract(Globals::GetVaultKey("default"));
		const bool isValid = ((not pair.empty()) and valuePredicate(pair.mapped()));

		if (isValid)
			defaultValue = pair.mapped();

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogLongIndent((isValid) ? "Valid default value:" : "No valid default value, using", defaultValue);

		// Remove any invalid pairs
		auto   iterator   = map.begin();
		size_t numRemoved = 0;

		while (iterator != map.end())
		{
			if (not (keyPredicate(iterator->first) and valuePredicate(iterator->second)))
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (numRemoved == 0)
						Globals::logger.LogLongIndent(mapName, "map");

					Globals::logger.LogLongIndent("  -", iterator->first, iterator->second);
				}

				iterator = map.erase(iterator);

				numRemoved++;
			}
			else iterator++;
		}

		if constexpr (Globals::loggingEnabled)
		{
			if (numRemoved > 0)
				Globals::logger.LogLongIndent("  pairs left:", (int)map.size());

			else
				Globals::logger.LogLongIndent(mapName, "pairs:", (int)map.size());
		}
	}
}