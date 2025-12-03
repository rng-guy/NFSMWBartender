#pragma once

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
	using VaultMap = FastMap<vault, V>;





	// HashContainer classes ------------------------------------------------------------------------------------------------------------------------

	template <typename K, typename V>
	class BaseCachedMap : public FastMap<K, V>
	{
	protected:

		K cachedKey    = K();
		V defaultValue = V();


		explicit BaseCachedMap() = default;



	public:

		void Validate
		(
			const char* const mapName,
			const auto&       IsValidKey,
			const auto&       IsValidValue
		) {
			// Extract "default" key if provided (and valid)
			const auto foundPair = this->find(Globals::GetVaultKey("default"));

			if (foundPair != this->end())
			{
				if (IsValidValue(foundPair->second))
				{
					this->defaultValue = foundPair->second;

					if constexpr (Globals::loggingEnabled)
						Globals::logger.LogLongIndent("Valid default value:", this->defaultValue);
				}
				else if constexpr (Globals::loggingEnabled)
					Globals::logger.LogLongIndent("Invalid default value, using", this->defaultValue);

				this->erase(foundPair);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent("No default value, using", this->defaultValue);

			// Remove any invalid pairs
			auto   iterator   = this->begin();
			size_t numRemoved = 0;

			while (iterator != this->end())
			{
				if (not (IsValidKey(iterator->first) and IsValidValue(iterator->second)))
				{
					// With logging disabled, the compiler optimises the string literal away
					if constexpr (Globals::loggingEnabled)
					{
						if (numRemoved == 0)
							Globals::logger.LogLongIndent(mapName, "map");

						Globals::logger.LogLongIndent("  -", iterator->first, iterator->second);
					}

					iterator = this->erase(iterator);

					++numRemoved;
				}
				else ++iterator;
			}

			if constexpr (Globals::loggingEnabled)
			{
				if (numRemoved > 0)
					Globals::logger.LogLongIndent("  pairs left:", static_cast<int>(this->size()));

				else
					Globals::logger.LogLongIndent(mapName, "pairs:", static_cast<int>(this->size()));
			}
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
	requires Globals::isTrivial<V>
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
}