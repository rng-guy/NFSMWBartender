#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <unordered_map>

#include "Globals.h"



namespace HashContainers
{

	// Custom hash function and (scoped) aliases ----------------------------------------------------------------------------------------------------

	template <typename K, typename H = std::hash<K>>
	using Set = std::unordered_set<K, H>;

	using AddressSet = Set<address>;
	using VaultSet   = Set<vault>;



	template <typename K, typename V, typename H = std::hash<K>>
	using Map = std::unordered_map<K, V, H>;

	template <typename V>
	using AddressMap = Map<address, V>;

	template <typename V>
	using VaultMap = Map<vault, V>;





	// HashContainer classes ------------------------------------------------------------------------------------------------------------------------

	struct AlwaysTrue
	{
		constexpr bool operator()(const auto&) const noexcept 
		{
			return true; 
		}
	};



	template <typename T, class Converter = std::identity, class Validator = AlwaysTrue>
	struct FillSetup
	{
		std::vector<T>& rawValues;

		Converter RawToValue   = {};
		Validator IsValidValue = {};
	};



	template <typename K, typename V>
	class BaseCachedMap
	{
	protected:

		V defaultValue;
		
		std::optional<K> cachedKey;

		Map<K, V> map;


		explicit BaseCachedMap(const V& defaultValue) : defaultValue(defaultValue) {}



	public:

		template <typename RawK, class ConK, class ValK, typename RawV, class ConV, class ValV>
		bool FillFromVectors
		(
			const std::string_view              mapName,
			const K&                            defaultKey,
			const FillSetup<RawK, ConK, ValK>&& keySetup,
			const FillSetup<RawV, ConV, ValV>&& valueSetup
		) {
			this->map.clear();

			bool mapIsValid = false;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(mapName, "map:");

			const auto& rawKeys   = keySetup  .rawValues;
			const auto& rawValues = valueSetup.rawValues;

			if ((not rawKeys.empty() and (rawKeys.size() == rawValues.size())))
			{
				bool defaultProvided = false;

				const size_t numPairs = rawKeys.size();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>(static_cast<int>(numPairs), "pair(s) provided");

				for (size_t pairID = 0; pairID < numPairs; ++pairID)
				{
					const RawK& rawKey = rawKeys[pairID];
					const V     value  = valueSetup.RawToValue(rawValues[pairID]);

					if (valueSetup.IsValidValue(value))
					{
						const K key = keySetup.RawToValue(rawKey);

						if (key == defaultKey)
						{
							defaultProvided = true;

							this->defaultValue = std::move(value);
						}
						else if (keySetup.IsValidValue(key))
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


		const V* GetValue(const K key) noexcept
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


		V GetValue(const K key) noexcept
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