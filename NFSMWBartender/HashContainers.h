#pragma once

#include <memory>
#include <utility>
#include <concepts>

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
	using SafeAddressMap = AddressMap<std::unique_ptr<V>>;

	template <typename V>
	using VaultMap = FastMap<vault, V>;

	template <typename V>
	using SafeVaultMap = VaultMap<std::unique_ptr<V>>;





	// HashContainer classes ------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	class BaseCachedMap : public FastMap<K, V>
	{
	protected:

		K cachedKey    = K();
		V defaultValue = V();


		explicit BaseCachedMap() = default;



	public:

		template <typename T, typename U>
		bool FillFromVectors
		(
			const char* const     mapName,
			const T&              rawDefaultHandle,
			const std::vector<T>& rawKeys,
			const auto&           RawToKey,
			const auto&           IsValidKey,
			const std::vector<U>& rawValues,
			const auto&           RawToValue,
			const auto&           IsValidValue			
		) {
			bool mapIsValid = false;

			// With logging disabled, the compiler optimises "mapName" away
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent(mapName, "map:");

			if ((not rawKeys.empty() and (rawKeys.size() == rawValues.size())))
			{
				bool defaultProvided = false;

				const size_t numPairs = rawKeys.size();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.LogLongIndent(' ', static_cast<int>(numPairs), "pair(s) provided");

				for (size_t pairID = 0; pairID < numPairs; ++pairID)
				{
					const T& rawKey = rawKeys[pairID];
					const V  value  = RawToValue(rawValues[pairID]);

					if (IsValidValue(value))
					{
						const K key = RawToKey(rawKey);

						if (rawKey == rawDefaultHandle)
						{
							defaultProvided    = true;
							this->defaultValue = value;;
						}
						else if (IsValidKey(key))
							this->try_emplace(std::move(key), std::move(value));

						else if constexpr (Globals::loggingEnabled)
							Globals::logger.LogLongIndent("  -", rawKey, "(invalid key)");
					}
					else if constexpr (Globals::loggingEnabled)
						Globals::logger.LogLongIndent("  -", rawKey, "(invalid value)");
				}

				if (defaultProvided or (not this->empty()))
				{
					mapIsValid = true;

					if constexpr (Globals::loggingEnabled)
					{
						Globals::logger.LogLongIndent(' ', static_cast<int>(this->size()) + defaultProvided, "pair(s) valid");
						Globals::logger.LogLongIndent("  default value:", this->defaultValue);
					}
				}
				else if constexpr (Globals::loggingEnabled)
					Globals::logger.LogLongIndent("  no valid pair(s)");
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent("  no pair(s) provided");

			return mapIsValid;
		}
	};



	template <typename K, typename V>
	class CachedMap : public BaseCachedMap<K, V>
	{
	private:

		const V* cachedValue = &(this->defaultValue);



	public:

		explicit CachedMap(const V& defaultValue)
		{
			this->defaultValue = defaultValue;
		}


		const V* GetValue(const K key)
		{
			if (key != this->cachedKey)
			{
				const auto foundPair = this->find(key);

				this->cachedKey   = key;
				this->cachedValue = &((foundPair != this->end()) ? foundPair->second : this->defaultValue);
			}

			return this->cachedValue;
		}
	};



	template <typename K, typename V>
	requires (std::is_trivially_copyable_v<V> and (sizeof(V) <= sizeof(uint32_t)))
	class CachedMap<K, V> : public BaseCachedMap<K, V>
	{
	private:

		V cachedValue;



	public:

		explicit CachedMap(const V defaultValue) : cachedValue(defaultValue)
		{
			this->defaultValue = defaultValue;
		}


		V GetValue(const K key)
		{
			if (key != this->cachedKey)
			{
				const auto foundPair = this->find(key);

				this->cachedKey   = key;
				this->cachedValue = (foundPair != this->end()) ? foundPair->second : this->defaultValue;
			}
			
			return this->cachedValue;
		}
	};





	// Derived (scoped) aliases ---------------------------------------------------------------------------------------------------------------------

	template <typename V>
	using CachedAddressMap = CachedMap<address, V>;

	template <typename V>
	using CachedVaultMap = CachedMap<vault, V>;
}