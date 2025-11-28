#pragma once

#include <array>
#include <cstring>
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
			memoryLocation = (void*)location;

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

			void* memoryLocation  = (void*)start;
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
			Write<byte>   (0xE9,                          {start});
			Write<address>((address)target - (start + 5), {start + 1});
		}
		else ++numCaveErrors;
	}





	// PointerCache class ---------------------------------------------------------------------------------------------------------------------------

	template <int offset, size_t capacity>
	class PointerCache
	{
	private:

		const address base;
		
		std::array<void*, capacity> pointers = {};


		static PointerCache*& GetStorage(const address base)
		{
			return *((PointerCache**)(base + offset));
		}


		static PointerCache* GetAccess(const address base)
		{
			PointerCache*& storage = PointerCache::GetStorage(base);

			if constexpr (Globals::loggingEnabled)
			{
				if (not storage)
					Globals::logger.Log("WARNING: [PCA] Failed to access cache at", base);
			}

			return storage;
		}



	public:

		explicit PointerCache(const address base) : base(base)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "PointerCache");

			PointerCache*& storage = this->GetStorage(this->base);

			if (not storage)
				storage = this;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PCA] Existing cache at", this->base);
		}


		~PointerCache()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "PointerCache");

			PointerCache*& storage = this->GetStorage(this->base);

			if (storage == this)
				storage = nullptr;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PCA] Cache mismatch at", this->base);
		}


		template <size_t index>
		static void SetPointer
		(
			const address base,
			void* const   newValue
		) {
			PointerCache* const access = PointerCache::GetAccess(base);

			if (not access) return;

			static_assert(index < capacity);
			void*& pointer = access->pointers[index];

			if (not pointer)
				pointer = newValue;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [PCA] Pointer", (int)index, "already", pointer, "at", base);
		}


		template <size_t index>
		static void ClearPointer
		(
			const address base,
			void* const   oldValue
		) {
			PointerCache* const access = PointerCache::GetAccess(base);

			if (not access) return;

			static_assert(index < capacity);
			void*& pointer = access->pointers[index];

			if (pointer == oldValue)
				pointer = nullptr;

			else if constexpr (Globals::loggingEnabled)
			{
				if (not pointer)
					Globals::logger.Log("WARNING: [PCA] Pointer", (int)index, "not set at", base);

				else
					Globals::logger.Log("WARNING: [PCA] Pointer", (int)index, "set to", pointer, "at", base);
			}
		}


		template <size_t index, typename T>
		static T* GetPointer(const address base)
		{
			PointerCache* const access = PointerCache::GetAccess(base);

			if (not access) return nullptr;

			static_assert(index < capacity);

			if constexpr (Globals::loggingEnabled)
			{
				if (not access->pointers[index])
					Globals::logger.Log("WARNING: [PCA] Pointer", (int)index, "not set at", base);
			}

			return (T*)(access->pointers[index]);
		}
	};
}