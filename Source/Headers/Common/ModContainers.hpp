#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <concepts>
#include <type_traits>

#include "Globals.hpp"

#include "../Utilities/MemoryTools.hpp"
#include "../Utilities/FlatContainers.hpp"



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
		using ReturnType = std::invoke_result_t<Converter, const RawT&>;


	// Members

		const std::vector<RawT>& source; // can't be std::span because of std::vector<bool>...

		[[no_unique_address]] Converter Convert;
		[[no_unique_address]] Validator IsValid;


	// Methods

		std::optional<ReturnType> Parse(const size_t valueID) const
		{
			const auto result = this->Convert(this->source[valueID]);
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
		concept IsCompatibleReturnType = std::convertible_to<typename Setup::ReturnType, ReturnT>;

		template <class Setup, typename ReturnT>
		concept IsCompatibleSetup = (IsFillSetup<Setup>::value and IsCompatibleReturnType<Setup, ReturnT>);


		template <class KeySetup, typename DefaultK>
		concept IsCompatibleDefaultKeyType = std::equality_comparable_with<typename KeySetup::RawType, DefaultK>;

		template <class KeySetup, typename K, typename DefaultK>
		concept IsCompatibleKeySetup = (IsCompatibleSetup<KeySetup, K> and IsCompatibleDefaultKeyType<KeySetup, DefaultK>);
	}





	// DefaultMap class -----------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	requires (std::is_trivially_copyable_v<K> and std::is_trivially_copyable_v<V>)
	class DefaultMap : protected Map<K, V>
	{
	private: // members

		V defaultValue;

		[[no_unique_address]] LogLiteral name;


	private: // methods

		template <typename DefaultK, class KeySetup, class ValueSetup>
		bool FillFromPairs
		(
			const DefaultK&  defaultKey,
			const KeySetup   keySetup,
			const ValueSetup valueSetup
		) {
			const size_t numPairs = std::min<size_t>(keySetup.source.size(), valueSetup.source.size());

			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail(LogDec(numPairs), "pair(s) provided");

			this->reserve(this->size() + numPairs);

			// Parse key-value pairs
			bool hasNewDefault = false;

			for (size_t pairID = 0; pairID < numPairs; ++pairID)
			{
				const auto& rawKey = keySetup.source[pairID];

				// Parse value
				const auto value = valueSetup.Parse(pairID);

				if (not value)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::LogDetail('-', rawKey, "(invalid value)");

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
						Globals::LogDetail('-', rawKey, "(duplicate default)");

					continue; // default key
				}

				// Parse as regular key
				const auto key = keySetup.Parse(pairID);

				if (not key)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::LogDetail('-', rawKey, "(invalid key)");

					continue; // invalid key
				}

				// Insert key-vaue pair
				const auto [pairIt, isNewPair] = this->try_emplace(*key, *value);

				if constexpr (Globals::loggingEnabled)
				{
					if (not isNewPair)
						Globals::LogDetail('-', rawKey, "(duplicate key)");
				}
			}

			this->shrink_to_fit();

			return hasNewDefault;
		}


	public: // methods

		constexpr DefaultMap
		(
			const LogLiteral name, 
			const V          defaultValue
		) 
			: name(name), defaultValue(defaultValue) 
		{
		}


		template <typename DefaultK, class KeySetup, class ValueSetup>
		requires (Details::IsCompatibleKeySetup<KeySetup, K, DefaultK> and Details::IsCompatibleSetup<ValueSetup, V>)
		bool Fill
		(
			const DefaultK&  defaultKey,
			const KeySetup   keySetup,
			const ValueSetup valueSetup
		) {
			this->clear();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain(this->name);

			if (keySetup.source.empty() or valueSetup.source.empty())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogDetail("no pair(s) provided");

				return false; // no pair(s)
			}

			const bool   hasNewDefault = this->FillFromPairs(defaultKey, keySetup, valueSetup);
			const size_t numValidPairs = this->size() + hasNewDefault;

			if (numValidPairs == 0)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogDetail("no pair(s) valid");

				return false; // no valid pair(s)
			}

			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogDetail(LogDec(numValidPairs), "pair(s) valid");

				if (hasNewDefault)
					Globals::LogDetail("new default:", this->defaultValue);
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


		[[nodiscard]] LogLiteral GetName() const
		{
			return this->name;
		}
	};





	// Scoped aliases (cont.) -----------------------------------------------------------------------------------------------------------------------

	#define DEFAULT_VAULT_MAP(type, name, ...) ModContainers::DefaultVaultMap<type> name{#name, __VA_ARGS__}

	template <typename V>
	using DefaultVaultMap = DefaultMap<vault, V>;



	#define DEFAULT_ADDRESS_MAP(type, name, ...) ModContainers::DefaultAddressMap<type> name{#name, __VA_ARGS__}

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

	template <class Base>
	class PointerStorage
	{
	private: // members

		std::vector<std::unique_ptr<Base>> pointers;


	public: // methods

		template <class Derived = Base, typename ...ValArgs>
		requires std::derived_from<Derived, Base>
		void EmplaceObject(ValArgs&&... args)
		{
			this->pointers.push_back(std::make_unique<Derived>(std::forward<ValArgs>(args)...));
		}


		auto EraseObject(const decltype(pointers)::const_iterator cit)
		{
			return this->pointers.erase(cit);
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