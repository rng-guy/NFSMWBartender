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
		const SIZE_T dataSize = sizeof(data);

		DWORD previousSetting = 0;
		void* memory          = nullptr;

		(
			[&]
			{
				memory = (void*)locations;

				VirtualProtect(memory, dataSize, PAGE_READWRITE, &previousSetting);
				memcpy(memory, &data, dataSize);
				VirtualProtect(memory, dataSize, previousSetting, &previousSetting);
			}
		(), ...);
	}



	void WriteToByteRange
	(
		const BYTE     value,
		const address  start,
		const SIZE_T   byteRange
	) {
		void* memory = (void*)start;

		DWORD previousSetting;

		VirtualProtect(memory, byteRange, PAGE_READWRITE, &previousSetting);
		memset(memory, value, byteRange);
		VirtualProtect(memory, byteRange, previousSetting, &previousSetting);
	}



	void WriteToAddressRange
	(
		const BYTE     value,
		const address  start,
		const address  end
	) {
		WriteToByteRange(value, start, end - start);
	}



	void DigCodeCave
	(
		const void*   code,
		const address entrance,
		const address exit
	) {
		static constexpr BYTE opcodeNOP = 0x90;
		static constexpr BYTE opcodeJMP = 0xE9; // JMP rel16
		static constexpr int  offsetJMP = 5;    // JMP instruction length

		WriteToAddressRange(opcodeNOP, entrance, exit);
		Write<BYTE>(opcodeJMP, entrance);
		Write<address>((address)code - (entrance + offsetJMP), entrance + sizeof(opcodeJMP));
	}
}