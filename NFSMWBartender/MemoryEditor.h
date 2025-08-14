#pragma once

#include <Windows.h>

#include "Globals.h"



namespace MemoryEditor
{

	template <typename T, typename ...address>
	void Write
	(
		const T          data,
		const address ...locations
	) {
		constexpr size_t size = sizeof(T);

		void* memoryLocation  = nullptr;
		DWORD previousSetting = 0x0;

		(
			[&]
			{
				memoryLocation = (void*)locations;

				VirtualProtect(memoryLocation, size,  PAGE_READWRITE,  &previousSetting);
				memcpy        (memoryLocation, &data, size);
				VirtualProtect(memoryLocation, size,  previousSetting, &previousSetting);
			}
		(), ...);
	}



	void WriteToByteRange
	(
		const byte    value,
		const address start,
		const size_t  byteRange
	) {
		void* memoryLocation  = (void*)start;
		DWORD previousSetting = 0x0;

		VirtualProtect(memoryLocation, byteRange, PAGE_READWRITE,  &previousSetting);
		memset        (memoryLocation, value,     byteRange);
		VirtualProtect(memoryLocation, byteRange, previousSetting, &previousSetting);
	}



	void WriteToAddressRange
	(
		const byte    value,
		const address start,
		const address end
	) {
		WriteToByteRange(value, start, end - start);
	}



	void DigCodeCave
	(
		const void*   function,
		const address entrance,
		const address exit
	) {
		WriteToAddressRange(0x90, entrance, exit); // NOP
		Write<byte>        (0xE9, entrance);       // JMP rel16

		// It is necessary to account for the instruction length and offset
		Write<address>((address)function - (entrance + 5), entrance + 1);
	}
}