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
		constexpr size_t numBytes = sizeof(T);

		for (const address location : locations)
		{
			DWORD previousSetting = 0x0;
			auto  memoryLocation  = reinterpret_cast<void*>(location);

			VirtualProtect(memoryLocation, numBytes, PAGE_READWRITE,  &previousSetting);
			std::memcpy   (memoryLocation, &data,    numBytes);
			VirtualProtect(memoryLocation, numBytes, previousSetting, &previousSetting);
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

			DWORD previousSetting = 0x0;
			auto  memoryLocation  = reinterpret_cast<void*>(start);

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





	// DataCache class ------------------------------------------------------------------------------------------------------------------------------

	template <size_t numBytes, size_t numEntries, int storageOffset = 0>
	class DataCache
	{
	private:

		const address storageBase;
		
		std::array<std::array<byte, numBytes>, numEntries> memory = {};


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
				Globals::logger.Log("WARNING: [DAT] Existing cache at", this->storageBase);
		}


		~DataCache()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "DataCache");

			DataCache*& storage = this->GetStorage(this->storageBase);

			if (storage == this)
				storage = nullptr;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [DAT] Cache mismatch at", this->storageBase);
		}


		template <size_t index, typename T>
		requires ((index < numEntries) and (sizeof(T) <= numBytes) and std::is_trivially_copyable_v<T>)
		static bool SetValue
		(
			const T       value,
			const address storageBase
		) {
			DataCache* const access = DataCache::GetAccess(storageBase);

			if (access)
				std::memcpy((access->memory[index]).data(), &value, sizeof(T));

			return access;
		}


		template <size_t index, typename T>
		requires ((index < numEntries) and (sizeof(T) <= numBytes) and std::is_trivially_copyable_v<T>)
		static T GetValue(const address storageBase)
		{
			const DataCache* const access = DataCache::GetAccess(storageBase);

			T value = T(); // T() = nullptr if T is pointer

			if (access)
				std::memcpy(&value, (access->memory[index]).data(), sizeof(T));

			return value; 
		}
	};
}