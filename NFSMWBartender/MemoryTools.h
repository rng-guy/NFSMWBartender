#pragma once

#include <array>
#include <cstring>
#include <concepts>
#include <Windows.h>
#include <initializer_list>

#include "Globals.h"



namespace MemoryTools
{

	// Status variables -----------------------------------------------------------------------------------------------------------------------------

	size_t numRangeErrors = 0;
	size_t numCaveErrors  = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	void Write
	(
		const T&                              data,
		const std::initializer_list<address>& locations
	) {
		constexpr size_t size = sizeof(T);

		void* memoryLocation  = nullptr;
		DWORD previousSetting = 0x0;

		for (const address location : locations)
		{
			memoryLocation = reinterpret_cast<void*>(location);

			VirtualProtect(memoryLocation, size,  PAGE_READWRITE,  &previousSetting);
			std::memcpy   (memoryLocation, &data, size);
			VirtualProtect(memoryLocation, size,  previousSetting, &previousSetting);
		}
	}



	void WriteToRange
	(
		const byte    value,
		const address start,
		const address end
	) {
		if (end > start)
		{
			const size_t numBytes = end - start;

			auto  memoryLocation  = reinterpret_cast<void*>(start);
			DWORD previousSetting = 0x0;

			VirtualProtect(memoryLocation, numBytes, PAGE_READWRITE, &previousSetting);
			std::memset   (memoryLocation, value,    numBytes);
			VirtualProtect(memoryLocation, numBytes, previousSetting, &previousSetting);
		}
		else ++numRangeErrors;
	}



	void MakeRangeNOP
	(
		const address start,
		const address end
	) {
		WriteToRange(0x90, start, end);
	}



	void MakeRangeJMP
	(
		const void* const target,
		const address     start,
		const address     end
	) {
		if (end > start + 4)
		{
			MakeRangeNOP(start, end);
			
			// It's necessary to account for the instruction length and offset
			Write<byte>   (0xE9, {start});
			Write<address>(reinterpret_cast<address>(target) - (start + 5), {start + 1});
		}
		else ++numCaveErrors;
	}





	// PointerCache class ---------------------------------------------------------------------------------------------------------------------------

	template <int storageOffset, size_t capacity, typename S = uint32_t>
	requires std::is_integral_v<S>
	class DataCache
	{
	private:

		const address storageBase;
		
		std::array<S, capacity> data = {};


		static DataCache*& GetStorage(const address storageBase)
		{
			return *reinterpret_cast<DataCache**>(storageBase + storageOffset);
		}


		static DataCache* GetAccess(const address storageBase)
		{
			DataCache*& storage = DataCache::GetStorage(storageBase);

			if constexpr (Globals::loggingEnabled)
			{
				if (not storage)
					Globals::logger.Log("WARNING: [DAT] Failed to access cache at", storageBase);
			}

			return storage;
		}



	public:

		explicit DataCache(const address storageBase) : storageBase(storageBase)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "DataCache");

			DataCache*& storage = this->GetStorage(this->storageBase);

			if (not storage)
				storage = this;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PCA] Existing cache at", this->storageBase);
		}


		~DataCache()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "DataCache");

			DataCache*& storage = this->GetStorage(this->storageBase);

			if (storage == this)
				storage = nullptr;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PCA] Cache mismatch at", this->storageBase);
		}


		template <size_t index, typename T>
		requires ((index < capacity) and (sizeof(T) <= sizeof(S)))
		static bool SetValue
		(
			const address storageBase,
			const T       value
		) {
			DataCache* const access = DataCache::GetAccess(storageBase);

			if (access)
				*reinterpret_cast<T*>(&(access->data[index])) = value;

			return access;
		}


		template <size_t index, typename T>
		requires ((index < capacity) and (sizeof(T) <= sizeof(S)))
		static T* GetPointer(const address storageBase)
		{
			DataCache* const access = DataCache::GetAccess(storageBase);
			return (access) ? reinterpret_cast<T*>(&(access->data[index])) : nullptr;
		}


		template <size_t index, typename T>
		requires ((index < capacity) and (sizeof(T) <= sizeof(S)))
		static T GetValue(const address storageBase)
		{
			const auto pointer = DataCache::GetPointer<index, T>(storageBase);
			return (pointer) ? *pointer : T();
		}
	};
}