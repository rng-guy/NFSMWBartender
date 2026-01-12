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
	size_t numHookErrors  = 0;





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



	address MakeHook
	(
		const void* const function,
		const address     location
	) {
		const byte opcode = *reinterpret_cast<byte*>(location);

		if (opcode == 0xE8)
		{
			address oldCallOffset = 0x0;

			std::memcpy   (&oldCallOffset, reinterpret_cast<address*>(location + 1), sizeof(address));
			Write<address>(reinterpret_cast<address>(function) - (location + 5), {location + 1});

			return (oldCallOffset + (location + 5));
		}
		else ++numHookErrors;

		return 0x0;
	}
}