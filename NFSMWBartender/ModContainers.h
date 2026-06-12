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

	struct AlwaysValid
	{
		bool operator()(const auto&) const 
		{
			return true; 
		}
	};



	template <typename RawType, class Converter, class Validator>
	requires (std::invocable<Converter, RawType> and std::predicate<Validator, std::invoke_result_t<Converter, RawType>>)
	struct MapFillSetup
	{
		const std::vector<RawType>& rawData;

		Converter ConvertFromRaw{};
		Validator IsValidResult {};
	};





	// DefaultMap class -----------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V, bool returnReferences>
	requires (std::is_trivially_copyable_v<K> and (std::is_trivially_copyable_v<V> or returnReferences))
	class DefaultMap : protected Map<K, V>
	{
	private:

		V defaultValue;

		

	public:

		constexpr explicit DefaultMap(const V defaultValue) : defaultValue(defaultValue) {}

		
		[[nodiscard]] std::conditional_t<returnReferences, const V&, V> GetValue(const K key) const
		{
			const auto foundPair = this->find(key);
			return (foundPair != this->end()) ? foundPair->second : this->defaultValue;
		}


		template <typename RawK, class ConK, class ValK, typename RawV, class ConV, class ValV>
		bool FillFromVectors
		(
			const std::string_view               mapName,
			const K                              defaultKey,
			const MapFillSetup<RawK, ConK, ValK> keySetup,
			const MapFillSetup<RawV, ConV, ValV> valueSetup
		) {
			bool mapIsValid         = false;
			bool newDefaultProvided = false;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(mapName, "map:");

			const auto& rawKeys   = keySetup  .rawData;
			const auto& rawValues = valueSetup.rawData;

			const size_t numPairs = std::min<size_t>(rawKeys.size(), rawValues.size());

			this->clear();
			this->reserve(numPairs);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>(static_cast<int>(numPairs), "pair(s) provided");

			// Pair validation and insertion
			for (size_t pairID = 0; pairID < numPairs; ++pairID)
			{
				const K    key          = keySetup.ConvertFromRaw(rawKeys[pairID]);
				const bool isDefaultKey = (key == defaultKey);

				if (isDefaultKey or keySetup.IsValidResult(key))
				{
					const V value = valueSetup.ConvertFromRaw(rawValues[pairID]);

					if (valueSetup.IsValidResult(value)) // value valid
					{
						if (isDefaultKey) // default key
						{
							newDefaultProvided = true;
							this->defaultValue = std::move(value);
						}
						else this->try_emplace(key, std::move(value)); // regular key
					}
					else if constexpr (Globals::loggingEnabled) // value invalid
						Globals::logger.Log<3>('-', rawKeys[pairID], "(invalid value)");
				}
				else if constexpr (Globals::loggingEnabled) // key invalid
					Globals::logger.Log<3>('-', rawKeys[pairID], "(invalid key)");
			}

			// Size validation
			if (newDefaultProvided or (not this->empty()))
			{
				mapIsValid = true;

				if constexpr (Globals::loggingEnabled)
				{
					const size_t numValidPairs = (newDefaultProvided) ? (this->size() + 1) : this->size();

					Globals::logger.Log<3>(static_cast<int>(numValidPairs), "pair(s) valid");
					Globals::logger.Log<3>("default value:", this->defaultValue);
				}
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>("no pair(s) valid");

			this->shrink_to_fit();

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