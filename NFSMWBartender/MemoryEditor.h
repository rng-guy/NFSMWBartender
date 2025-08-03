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

		void* memory          = nullptr;
		DWORD previousSetting = 0x0;

		(
			[&]
			{
				memory = (void*)locations;

				VirtualProtect(memory, size, PAGE_READWRITE, &previousSetting);
				memcpy(memory, &data, size);
				VirtualProtect(memory, size, previousSetting, &previousSetting);
			}
		(), ...);
	}



	void WriteToByteRange
	(
		const byte    value,
		const address start,
		const size_t  byteRange
	) {
		void* memory = (void*)start;

		DWORD previousSetting;

		VirtualProtect(memory, byteRange, PAGE_READWRITE, &previousSetting);
		memset(memory, value, byteRange);
		VirtualProtect(memory, byteRange, previousSetting, &previousSetting);
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
		const void*   code,
		const address entrance,
		const address exit
	) {
		WriteToAddressRange(0x90, entrance, exit);                    // NOP
		Write<byte>(0xE9, entrance);                                  // JMP rel16
		Write<address>((address)code - (entrance + 5), entrance + 1); // accounting for instruction length
	}
}