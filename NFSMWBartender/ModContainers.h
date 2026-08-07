#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <concepts>
#include <string_view>
#include <type_traits>

#include "Globals.h"
#include "MemoryTools.h"
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





	// DefaultMap helpers ---------------------------------------------------------------------------------------------------------------------------

	struct IdentityCopy
	{
	// Methods

		template <typename T>
		T operator()(const T& value) const
		{
			return value;
		}
	};


	struct AlwaysValid
	{
	// Methods

		bool operator()(const auto&) const 
		{
			return true; 
		}
	};



	template <typename RawT, class Converter, class Validator>
	requires (std::invocable<Converter, const RawT&> and std::predicate<Validator, std::invoke_result_t<Converter, const RawT&>>)
	struct FillSetup
	{
	// Aliases

		using RawType    = RawT;
		using ReturnType = std::invoke_result_t<Converter, const RawType&>;


	// Members

		const std::vector<RawType>& data; // can't be std::span because of std::vector<bool>...

		[[no_unique_address]] Converter Convert{};
		[[no_unique_address]] Validator IsValid{};


	// Methods

		std::optional<ReturnType> Parse(const size_t valueID) const
		{
			const ReturnType result = this->Convert(this->data[valueID]);
			if (not this->IsValid(result)) return std::nullopt;

			return result;
		}
	};



	namespace Details
	{
		template <typename T> 
		struct IsFillSetup : std::false_type {};

		template <typename RawType, class Converter, class Validator> 
		struct IsFillSetup<FillSetup<RawType, Converter, Validator>> : std::true_type {};


		template <class Setup, typename ReturnT>
		concept IsCompatibleSetup = (IsFillSetup<Setup>::value and std::convertible_to<typename Setup::ReturnType, ReturnT>);

		template <class KeySetup, typename K, typename DefaultK>
		concept IsCompatibleKeySetup = (IsCompatibleSetup<KeySetup, K> and std::equality_comparable_with<typename KeySetup::RawType, DefaultK>);
	}





	// DefaultMap class -----------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	requires (std::is_trivially_copyable_v<K> and std::is_trivially_copyable_v<V>)
	class DefaultMap : protected Map<K, V>
	{
	private: // members

		V defaultValue;


	private: // methods

		template <typename DefaultK, class KeySetup, class ValueSetup>
		bool FillFromPairs
		(
			const DefaultK&   defaultKey,
			const KeySetup&   keySetup,
			const ValueSetup& valueSetup
		) {
			const size_t numPairs = std::min<size_t>(keySetup.data.size(), valueSetup.data.size());

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<3>(DecFormat(numPairs), "pair(s) provided");

			this->reserve(this->size() + numPairs);

			// Parse key-value pairs
			bool hasNewDefault = false;

			for (size_t pairID = 0; pairID < numPairs; ++pairID)
			{
				const auto& rawKey = keySetup.data[pairID];

				// Parse value
				const auto value = valueSetup.Parse(pairID);

				if (not value)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<3>('-', rawKey, "(invalid value)");

					continue; // invalid value
				}

				// Check for default key
				if (rawKey == defaultKey)
				{
					if (not hasNewDefault)
					{
						hasNewDefault      = true;
						this->defaultValue = *value;
					}
					else if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<3>('-', rawKey, "(duplicate default)");

					continue; // default key
				}

				// Parse as regular key
				const auto key = keySetup.Parse(pairID);

				if (not key)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<3>('-', rawKey, "(invalid key)");

					continue; // invalid key
				}

				// Insert key-vaue pair
				const auto [pairIt, isNewPair] = this->try_emplace(*key, *value);

				if constexpr (Globals::loggingEnabled)
				{
					if (not isNewPair)
						Globals::logger.Log<3>('-', rawKey, "(duplicate key)");
				}
			}

			this->shrink_to_fit();

			return hasNewDefault;
		}


	public: // methods

		constexpr explicit DefaultMap(const V defaultValue) : defaultValue(defaultValue) {}


		template <typename DefaultK, class KeySetup, class ValueSetup>
		requires (Details::IsCompatibleKeySetup<KeySetup, K, DefaultK> and Details::IsCompatibleSetup<ValueSetup, V>)
		bool Fill
		(
			const std::string_view  mapName,
			const DefaultK&         defaultKey,
			const KeySetup&         keySetup,
			const ValueSetup&       valueSetup
		) {
			this->clear();

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>(mapName, "map:");

			if (keySetup.data.empty() or valueSetup.data.empty())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>("no pair(s) provided");

				return false; // no pair(s)
			}

			const bool   hasNewDefault = this->FillFromPairs(defaultKey, keySetup, valueSetup);
			const size_t numValidPairs = this->size() + hasNewDefault;

			if (numValidPairs == 0)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<3>("no pair(s) valid");

				return false; // no valid pair(s)
			}

			if constexpr (Globals::loggingEnabled)
			{
				Globals::logger.Log<3>(DecFormat(numValidPairs), "pair(s) valid");

				if (hasNewDefault)
					Globals::logger.Log<3>("new default:", this->defaultValue);
			}

			return true;
		}


		[[nodiscard]] const V& GetReference(const K key) const
		{
			const auto foundPair = this->find(key);
			if (foundPair == this->end()) return this->defaultValue;

			return foundPair->second;
		}


		[[nodiscard]] V GetValue(const K key) const
		{
			return this->GetReference(key);
		}
	};





	// DefaultMap aliases ---------------------------------------------------------------------------------------------------------------------------

	template <typename V>
	using DefaultVaultMap = DefaultMap<vault, V>;

	template <typename V>
	using DefaultAddressMap = DefaultMap<address, V>;





	// PursuitList class ----------------------------------------------------------------------------------------------------------------------------

	class PursuitList
	{
	private: // types

		class EntryIterator
		{
		private: // members

			address current;


		public: // methods

			explicit EntryIterator(const address entry) : current(entry) {}


			[[nodiscard]] address operator*() const
			{
				return AsReference<address>(this->current + 0x8);
			}


			EntryIterator& operator++()
			{
				this->current = AsReference<address>(this->current);

				return *this;
			}


			[[nodiscard]] bool operator==(const EntryIterator&) const = default;
		};


	private: // members

		address first    = 0x0;
		address sentinel = 0x0;


	public: // methods

		PursuitList()
		{
			if (not Globals::copManager) return;

			this->sentinel = AsReference<address>(Globals::copManager + 0x128);
			this->first    = AsReference<address>(sentinel);
		}


		[[nodiscard]] auto begin() const {return EntryIterator(this->first);}
		[[nodiscard]] auto end  () const {return EntryIterator(this->sentinel);}
	};





	// PointerStorage class -------------------------------------------------------------------------------------------------------------------------

	template <class ObjectBase>
	class PointerStorage
	{
	private: // aliases

		using Storage = std::vector<std::unique_ptr<ObjectBase>>;


	private: // members

		Storage pointers;


	public: // methods

		template <class Object = ObjectBase, typename... ValArgs>
		requires std::derived_from<Object, ObjectBase>
		void EmplaceObject(ValArgs&&... args)
		{
			this->pointers.push_back(std::make_unique<Object>(std::forward<ValArgs>(args)...));
		}


		auto EraseObject(const Storage::const_iterator it)
		{
			return this->pointers.erase(it);
		}


		void ReserveCapacity(const size_t capacity)
		{
			this->pointers.reserve(capacity);
		}


		[[nodiscard]] auto begin() {return this->pointers.begin();}
		[[nodiscard]] auto end  () {return this->pointers.end();}

		[[nodiscard]] auto begin() const {return this->pointers.begin();}
		[[nodiscard]] auto end  () const {return this->pointers.end();}
	};
}