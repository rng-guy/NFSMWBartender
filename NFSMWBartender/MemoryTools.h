#pragma once

#include <format>
#include <cstdarg>
#include <cstring>
#include <Windows.h>
#include <memoryapi.h>
#include <type_traits>
#include <libloaderapi.h>
#include <initializer_list>
#include <processthreadsapi.h>



namespace MemoryTools
{

	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using byte = unsigned char;
	using word = unsigned short;

	using address = uintptr_t;





	// Queries and byte-writing ---------------------------------------------------------------------------------------------------------------------

	inline bool IsModuleLoaded(const char* const name)
	{
		return GetModuleHandleA(name);
	}



	inline address GetEntryPoint()
	{
		// Credit: thelink2012 and MWisBest
		const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(NULL));

		const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
		const auto nt  = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);

		return nt->OptionalHeader.AddressOfEntryPoint;
	}



	template <typename T>
	requires std::is_trivially_copyable_v<T>
	inline void Write
	(
		const T                              data,
		const std::initializer_list<address> locations
	) {
		constexpr size_t numBytes = sizeof(T);

		for (const address location : locations)
		{
			DWORD previousSetting = 0x0;
			void* memoryLocation  = reinterpret_cast<void*>(location);

			VirtualProtect(memoryLocation, numBytes, PAGE_READWRITE,  &previousSetting);
			std::memcpy   (memoryLocation, &data,    numBytes);
			VirtualProtect(memoryLocation, numBytes, previousSetting, &previousSetting);
		}
	}





	// Helper functions -----------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		inline void WriteToRange
		(
			const byte    value,
			const address start,
			const address end
		) {
			const size_t numBytes = end - start;

			DWORD previousSetting = 0x0;
			void* memoryLocation  = reinterpret_cast<void*>(start);

			VirtualProtect(memoryLocation, numBytes, PAGE_READWRITE, &previousSetting);
			std::memset   (memoryLocation, value,    numBytes);
			VirtualProtect(memoryLocation, numBytes, previousSetting, &previousSetting);
		}



		inline void MakeRangeNOP
		(
			const address start,
			const address end
		) {
			WriteToRange(0x90, start, end); // NOP
		}



		inline void MakeRangeJMP
		(
			const address target,
			const address start,
			const address end
		) {
			const address targetStart = start       + sizeof(byte);
			const address jumpEnd     = targetStart + sizeof(address);

			MakeRangeNOP(start, end);

			Write<byte>   (0xE9, {start}); // jump near, relative
			Write<address>(target - jumpEnd, {targetStart});
		}
	}





	// Address-range writing and hooking ------------------------------------------------------------------------------------------------------------

	template <address start, address end>
	inline void WriteToRange(const byte value)
	{
		static_assert(end > start, "end must be > start.");

		Details::WriteToRange(value, start, end);
	}



	template <address start, address end>
	inline void MakeRangeNOP()
	{
		static_assert(end > start, "end must be > start.");

		Details::MakeRangeNOP(start, end);
	}



	template <address start, address end>
	inline void MakeRangeJMP(const address target)
	{
		static_assert(end >= start + sizeof(byte) + sizeof(address), "cannot accommodate JMP");

		Details::MakeRangeJMP(target, start, end);
	}


	template <address start, address end>
	inline void MakeRangeJMP(const void* const target)
	{
		MakeRangeJMP<start, end>(reinterpret_cast<address>(target));
	}



	inline address MakeCallHook
	(
		const address target,
		const address location
	) {
		const byte opcode = *reinterpret_cast<byte*>(location);

		if (opcode != 0xE8)
		{
			// Should never happen in any remotely tested release build
			MessageBoxA(NULL, std::format("Expected opcode 0xE8 at address {:08x}!", location).c_str(), "Fatal hooking error", MB_ICONERROR);

			TerminateProcess(GetCurrentProcess(), 1);
		}

		address replacedTarget = 0x0;

		const address targetStart = location    + sizeof(byte);
		const address callEnd     = targetStart + sizeof(address);

		std::memcpy(&replacedTarget, reinterpret_cast<address*>(targetStart), sizeof(address));

		Write<address>(target - callEnd, {targetStart});

		replacedTarget += callEnd;

		return replacedTarget;
	}


	inline address MakeCallHook
	(
		const void* const target,
		const address     location
	) {
		return MakeCallHook(reinterpret_cast<address>(target), location);
	}
}