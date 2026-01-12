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
		WriteToRange(0x90, start, end); // NOP
	}



	void MakeRangeJMP
	(
		const void* const target,
		const address     start,
		const address     end
	) {
		const address targetStart = start       + sizeof(byte);
		const address jumpEnd     = targetStart + sizeof(address);

		if (end >= jumpEnd)
		{
			MakeRangeNOP(start, end);
			
			Write<byte>   (0xE9, {start}); // jump near, relative
			Write<address>(reinterpret_cast<address>(target) - jumpEnd, {targetStart});
		}
		else ++numCaveErrors;
	}



	address MakeCallHook
	(
		const void* const target,
		const address     location
	) {
		address replacedTarget = 0x0;

		const byte opcode = *reinterpret_cast<byte*>(location);

		if (opcode == 0xE8) // call near, relative
		{
			const address targetStart = location    + sizeof(byte);
			const address callEnd     = targetStart + sizeof(address);

			std::memcpy(&replacedTarget, reinterpret_cast<address*>(targetStart), sizeof(address));

			replacedTarget += callEnd;

			Write<address>(reinterpret_cast<address>(target) - callEnd, {targetStart});
		}
		else ++numHookErrors;

		return replacedTarget;
	}
}