#pragma once

#include <memory>
#include <vector>
#include <concepts>
#include <functional>
#include <string_view>
#include <type_traits>

#include "Globals.h"
#include "FlatContainers.h"



namespace ModContainers
{

	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	template <typename K>
	using Set = FlatContainers::Set<K>;

	using AddressSet = Set<address>;
	using VaultSet   = Set<vault>;



	template <typename K, typename V>
	using Map = FlatContainers::Map<K, V>;

	template <typename V>
	using AddressMap = Map<address, V>;

	template <typename V>
	using VaultMap = Map<vault, V>;



	template <typename K, typename V>
	using StableMap = FlatContainers::Map<K, std::unique_ptr<V>>;

	template <typename V>
	using StableAddressMap = StableMap<address, V>;

	template <typename V>
	using StableVaultMap = StableMap<vault, V>;





	// Helper structs -------------------------------------------------------------------------------------------------------------------------------

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
		const std::vector<T>& rawValues;

		Converter RawToValue   = {};
		Validator IsValidValue = {};
	};





	// DefaultMap class -----------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V, bool returnReferences>
	requires (std::is_trivially_copyable_v<K> and std::is_trivially_copyable_v<V>)
	class DefaultMap
	{
	private:

		V defaultValue;

		Map<K, V> map;

		

	public:

		using ReturnType = std::conditional_t<returnReferences, const V&, V>;

		constexpr explicit DefaultMap(const V defaultValue) : defaultValue(defaultValue) {}


		
		ReturnType GetValue(const K key) const
		{
			const auto foundPair = this->map.find(key);

			return (foundPair != this->map.end()) ? foundPair->second : this->defaultValue;
		}


		template <typename RawK, class ConK, class ValK, typename RawV, class ConV, class ValV>
		requires (std::predicate<ValK, K> and std::predicate<ValV, V>)
		bool FillFromVectors
		(
			const std::string_view            mapName,
			const K&                          defaultKey,
			const FillSetup<RawK, ConK, ValK> keySetup,
			const FillSetup<RawV, ConV, ValV> valueSetup
		) {
			this->map.clear();

			bool mapIsValid = false;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(mapName, "map:");

			const auto& rawKeys   = keySetup  .rawValues;
			const auto& rawValues = valueSetup.rawValues;

			if ((not rawKeys.empty() and (rawKeys.size() == rawValues.size())))
			{
				const size_t numPairs = rawKeys.size();
				this->map.reserve(numPairs);

				bool defaultProvided = false;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>(static_cast<int>(numPairs), "pair(s) provided");

				// Key-value insertion
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

							this->defaultValue = value;
						}
						else if (keySetup.IsValidValue(key))
							this->map.try_emplace(key, value);

						else if constexpr (Globals::loggingEnabled)
							Globals::logger.Log<3>('-', rawKey, "(invalid key)");
					}
					else if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<3>('-', rawKey, "(invalid value)");
				}

				// Concluding validation
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

			this->map.shrink_to_fit();

			return mapIsValid;
		}
	};





	// Derived (scoped) aliases ---------------------------------------------------------------------------------------------------------------------

	template<typename K, typename V>
	using DefaultCopyMap = DefaultMap<K, V, false>;

	template <typename V>
	using DefaultCopyVaultMap = DefaultCopyMap<vault, V>;

	template <typename V>
	using DefaultCopyAddressMap = DefaultCopyMap<address, V>;



	template<typename K, typename V>
	using DefaultReferenceMap = DefaultMap<K, V, true>;

	template <typename V>
	using DefaultReferenceVaultMap = DefaultReferenceMap<vault, V>;

	template <typename V>
	using DefaultReferenceAddressMap = DefaultReferenceMap<address, V>;
}