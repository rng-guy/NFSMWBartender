#pragma once

#include <string>
#include <memory>
#include <utility>
#include <concepts>
#include <optional>

#include "Globals.h"
#include "unordered_dense.h"



namespace HashContainers
{

	// Custom hash function and (scoped) aliases ----------------------------------------------------------------------------------------------------

	template <typename K>
	struct IdentityHash
	{
		size_t operator()(const K key) const
		{
			return key;
		}
	};



	template <typename K, typename H = std::hash<K>>
	using Set = ankerl::unordered_dense::set<K, H>;

	template <typename K>
	requires std::is_integral_v<K>
	using FastSet = Set<K, IdentityHash<K>>;

	using AddressSet = FastSet<address>;
	using VaultSet   = FastSet<vault>;



	template <typename K, typename V, typename H = std::hash<K>>
	using Map = ankerl::unordered_dense::map<K, V, H>;

	template <typename K, typename V>
	requires std::is_integral_v<K>
	using FastMap = Map<K, V, IdentityHash<K>>;

	template <typename V>
	using AddressMap = FastMap<address, V>;

	template <typename V>
	using StableAddressMap = AddressMap<std::unique_ptr<V>>;

	template <typename V>
	using VaultMap = FastMap<vault, V>;

	template <typename V>
	using StableVaultMap = VaultMap<std::unique_ptr<V>>;





	// HashContainer classes ------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	class BaseCachedMap
	{
	protected:

		V defaultValue;

		std::optional<K> cachedKey;


		explicit BaseCachedMap(const V& defaultValue) : defaultValue(defaultValue) {}



	public:

		FastMap<K, V> map;


		template <typename T, typename U>
		bool FillFromVectors
		(
			const std::string&    mapName,
			const T&              rawDefaultHandle,
			const std::vector<T>& rawKeys,
			const auto&           RawToKey,
			const auto&           IsValidKey,
			const std::vector<U>& rawValues,
			const auto&           RawToValue,
			const auto&           IsValidValue
		) {
			this->map.clear();

			bool mapIsValid = false;

			// With logging disabled, the compiler optimises "mapName" away
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(mapName, "map:");

			if ((not rawKeys.empty() and (rawKeys.size() == rawValues.size())))
			{
				bool defaultProvided = false;

				const size_t numPairs = rawKeys.size();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>(static_cast<int>(numPairs), "pair(s) provided");

				for (size_t pairID = 0; pairID < numPairs; ++pairID)
				{
					const T& rawKey = rawKeys[pairID];
					const V  value  = RawToValue(rawValues[pairID]);

					if (IsValidValue(value))
					{
						const K key = RawToKey(rawKey);

						if (rawKey == rawDefaultHandle)
						{
							defaultProvided = true;

							this->defaultValue = std::move(value);
						}
						else if (IsValidKey(key))
							this->map.try_emplace(std::move(key), std::move(value));

						else if constexpr (Globals::loggingEnabled)
							Globals::logger.Log<3>('-', rawKey, "(invalid key)");
					}
					else if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<3>('-', rawKey, "(invalid value)");
				}

				if (defaultProvided or (not this->map.empty()))
				{
					mapIsValid = true;

					if constexpr (Globals::loggingEnabled)
					{
						Globals::logger.Log<3>(static_cast<int>(this->map.size()) + defaultProvided, "pair(s) valid");
						Globals::logger.Log<3>("default value:", this->defaultValue);
					}
				}
				else if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>("no pair(s) valid");
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no pair(s) provided");

			return mapIsValid;
		}
	};



	template <typename K, typename V>
	class CachedPointerMap : public BaseCachedMap<K, V>
	{
	private:

		const V* cachedValue = &(this->defaultValue);



	public:

		explicit CachedPointerMap(const V& defaultValue) : BaseCachedMap<K, V>(defaultValue) {}


		const V* GetValue(const K key)
		{
			if ((not this->cachedKey) or (key != *(this->cachedKey)))
			{
				this->cachedKey = key;

				const auto foundPair = this->map.find(key);

				this->cachedValue = &((foundPair != this->map.end()) ? foundPair->second : this->defaultValue);
			}

			return this->cachedValue;
		}
	};



	template <typename K, typename V>
	class CachedCopyMap : public BaseCachedMap<K, V>
	{
	private:

		V cachedValue = this->defaultValue;



	public:

		explicit CachedCopyMap(const V defaultValue) : BaseCachedMap<K, V>(defaultValue) {}


		V GetValue(const K key)
		{
			if ((not this->cachedKey) or (key != *(this->cachedKey)))
			{
				this->cachedKey = key;

				const auto foundPair = this->map.find(key);

				this->cachedValue = (foundPair != this->map.end()) ? foundPair->second : this->defaultValue;
			}
			
			return this->cachedValue;
		}
	};





	// Derived (scoped) aliases ---------------------------------------------------------------------------------------------------------------------

	template <typename V>
	using CachedPointerAddressMap = CachedPointerMap<address, V>;

	template <typename V>
	using CachedCopyAddressMap = CachedCopyMap<address, V>;

	template <typename V>
	using CachedPointerVaultMap = CachedPointerMap<vault, V>;

	template <typename V>
	using CachedCopyVaultMap = CachedCopyMap<vault, V>;
}