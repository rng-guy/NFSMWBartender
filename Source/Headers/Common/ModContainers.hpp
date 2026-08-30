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

	// Sets
	template <typename K>
	using Set = FlatContainers::Set<K>;

	using AddressSet = Set<address>;
	using VaultSet   = Set<vault>;



	// Regular maps
	template <typename K, typename V>
	using Map = FlatContainers::Map<K, V>;

	template <typename V>
	using AddressMap = Map<address, V>;

	template <typename V>
	using VaultMap = Map<vault, V>;



	// Pointer-stable maps
	template <typename K, typename V>
	using StableMap = FlatContainers::Map<K, std::unique_ptr<V>>;

	template <typename V>
	using StableAddressMap = StableMap<address, V>;

	template <typename V>
	using StableVaultMap = StableMap<vault, V>;





	// VehicleMap structs ---------------------------------------------------------------------------------------------------------------------------

	struct IdentityCopy
	{
	// Methods

		template <typename T>
		[[nodiscard]] std::decay_t<T> operator()(T&& value) const
		{
			return std::forward<T>(value);
		}
	};



	struct AlwaysValid
	{
	// Methods

		template <typename T>
		[[nodiscard]] bool operator()(T&&) const
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
		using ResultType = std::invoke_result_t<Converter, const RawT&>;


	// Members

		const std::vector<RawT>& source; // can't be std::span because of std::vector<bool>...

		[[no_unique_address]] Converter Convert;
		[[no_unique_address]] Validator IsValid;


	// Methods

		[[nodiscard]] std::optional<ResultType> Process(const size_t valueID) const
		{
			auto result = this->Convert(this->source[valueID]); // non-const for moving
			if (not this->IsValid(result)) return std::nullopt;

			return std::move(result);
		}
	};





	// VehicleMap concepts --------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{
		template <typename T> 
		struct IsFillSetup : std::false_type {};

		template <typename RawType, class Converter, class Validator> 
		struct IsFillSetup<FillSetup<RawType, Converter, Validator>> : std::true_type {};


		template <class Setup, typename ResultT>
		concept IsCompatibleResultType = std::convertible_to<typename Setup::ResultType, ResultT>;

		template <class Setup, typename ResultT>
		concept IsCompatibleSetup = (IsFillSetup<Setup>::value and IsCompatibleResultType<Setup, ResultT>);


		template <class KeySetup>
		concept IsCompatibleWithDefaultKey = std::equality_comparable_with<typename KeySetup::RawType, decltype(Globals::defaultKey)>;

		template <class KeySetup>
		concept IsCompatibleKeySetup = (IsCompatibleSetup<KeySetup, vault> and IsCompatibleWithDefaultKey<KeySetup>);
	}





	// VehicleMap class -----------------------------------------------------------------------------------------------------------------------------

	#define VEHICLE_MAP(type, name, ...) ModContainers::VehicleMap<type> name{#name, __VA_ARGS__}

	template <typename V>
	class VehicleMap : protected Map<vault, V>
	{
	private: // members

		V defaultValue;

		[[no_unique_address]] Globals::LogLiteral name;


	public: // methods

		constexpr VehicleMap
		(
			const Globals::LogLiteral name, 
			const V&                  defaultValue
		) 
			: name(name), defaultValue(defaultValue)
		{
		}


		constexpr VehicleMap
		(
			const Globals::LogLiteral name,
			V&&                       defaultValue
		)
			: name(name), defaultValue(std::move(defaultValue))
		{
		}


		template <class KeySetup, class ValueSetup>
		requires (Details::IsCompatibleKeySetup<KeySetup> and Details::IsCompatibleSetup<ValueSetup, V>)
		bool Fill
		(
			const KeySetup   keySetup,
			const ValueSetup valueSetup
		) {
			this->clear();

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain(this->name);

			// Validate vector sources
			if (keySetup.source.size() != valueSetup.source.size())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogDetail("sizes mismatched");

				ASSERT_UNREACHABLE_THEN(return false);
			}

			if (keySetup.source.empty()) 
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogDetail("no pair(s) provided");

				return false; // no pair(s)
			}

			// Reserve map capacity
			const size_t numPairs = keySetup.source.size();

			if constexpr (Globals::loggingEnabled)
				Globals::LogDetail(Globals::LogDec(numPairs), "pair(s) provided");

			this->reserve(numPairs);

			// Process key-value pairs
			bool hasNewDefault = false;

			for (size_t pairID = 0; pairID < numPairs; ++pairID)
			{
				const auto& rawKey = keySetup.source[pairID];

				// Process value (non-const for moving)
				auto value = valueSetup.Process(pairID);

				if (not value)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::LogDetail('-', rawKey, "(invalid value)");

					continue; // invalid value
				}

				// Check for default key
				if (rawKey == Globals::defaultKey)
				{
					if (not hasNewDefault)
					{
						hasNewDefault      = true;
						this->defaultValue = std::move(*value);
					}
					else if constexpr (Globals::loggingEnabled)
						Globals::LogDetail('-', rawKey, "(duplicate default)");

					continue; // default key
				}

				// Process as regular key
				const auto key = keySetup.Process(pairID);

				if (not key)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::LogDetail('-', rawKey, "(invalid key)");

					continue; // invalid key
				}

				// Insert regular key-value pair
				const auto [_, isNewPair] = this->insert(*key, std::move(*value));

				if constexpr (Globals::loggingEnabled)
				{
					if (not isNewPair)
						Globals::LogDetail('-', rawKey, "(duplicate key)");
				}
			}

			this->shrink_to_fit();

			// Check resulting map size
			const size_t numValidPairs = this->size() + hasNewDefault;

			if (numValidPairs == 0)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogDetail("no pair(s) valid");

				return false; // no valid pair(s)
			}

			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogDetail(Globals::LogDec(numValidPairs), "pair(s) valid");

				if (hasNewDefault)
					Globals::LogDetail("new default:", this->defaultValue);
			}

			return true;
		}


		[[nodiscard]] const V& GetReference(const vault key) const
		{
			const auto foundPair = this->find(key);
			if (foundPair == this->end()) return this->defaultValue;

			return foundPair->second;
		}


		[[nodiscard]] V GetValue(const vault key) const
		{
			return this->GetReference(key);
		}


		[[nodiscard]] auto GetName() const
		{
			return this->name;
		}
	};





	// PursuitList class ----------------------------------------------------------------------------------------------------------------------------

	class PursuitList
	{
	private: // types

		class EntryIterator
		{
		private: // members

			address current;


		public: // methods

			EntryIterator(const address entry) : current(entry) {}


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


		[[nodiscard]] EntryIterator begin() const {return this->first;}
		[[nodiscard]] EntryIterator end  () const {return this->sentinel;}
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


		void Reserve(const size_t capacity)
		{
			this->pointers.reserve(capacity);
		}


		[[nodiscard]] auto begin() {return this->pointers.begin();}
		[[nodiscard]] auto end  () {return this->pointers.end();}

		[[nodiscard]] auto begin() const {return this->pointers.begin();}
		[[nodiscard]] auto end  () const {return this->pointers.end();}
	};
}