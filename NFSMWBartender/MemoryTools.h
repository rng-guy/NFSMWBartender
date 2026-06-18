#pragma once

#include <Windows.h>
#include <memoryapi.h>
#include <libloaderapi.h>
#include <processthreadsapi.h>

#include <cstdarg>
#include <cstring>
#include <type_traits>
#include <initializer_list>




namespace MemoryTools
{

	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using byte = unsigned char;
	using word = unsigned short;

	using address = uintptr_t;





	// Queries and byte-writing ---------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] inline bool IsModuleLoaded(const char* const name)
	{
		return GetModuleHandleA(name);
	}



	[[nodiscard]] inline address GetEntryPoint()
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
			const address start,
			const address end,
			const address target
		) {
			const address jumpTargetOffset = start            + sizeof(byte);
			const address nextInstruction  = jumpTargetOffset + sizeof(int);

			MakeRangeNOP(start, end);

			Write<byte>(0xE9, {start}); // jump near, relative
			Write<int> (target - nextInstruction, {jumpTargetOffset});
		}
	}





	// Address-range writing and hooking ------------------------------------------------------------------------------------------------------------

	template <address start, address end>
	inline void WriteToRange(const byte value)
	{
		static_assert(end > start, "Invalid or empty range");
		Details::WriteToRange(value, start, end);
	}



	template <address start, address end>
	inline void MakeRangeNOP()
	{
		static_assert(end > start, "Invalid or empty range");
		Details::MakeRangeNOP(start, end);
	}



	template <address start, address end>
	inline void MakeRangeJMP(const address target)
	{
		static_assert(end >= start + sizeof(byte) + sizeof(int), "Cannot accommodate JMP");
		Details::MakeRangeJMP(start, end, target);
	}


	template <address start, address end>
	inline void MakeRangeJMP(const void* const target)
	{
		MakeRangeJMP<start, end>(reinterpret_cast<address>(target));
	}
	


	[[nodiscard]] inline address MakeCallHook
	(
		const address call,
		const address target
	) {
		const byte opcode = *reinterpret_cast<byte*>(call);

		if (opcode != 0xE8) // call near, relative
		{
			MessageBoxA(NULL, "Invalid hooking target. Contact the mod author.", "Fatal hooking error", MB_ICONERROR);
			TerminateProcess(GetCurrentProcess(), 1); // hooking failed; terminate process for safety
		}

		const address callOffset      = call       + sizeof(byte);
		const address nextInstruction = callOffset + sizeof(int);

		const int originalOffset = *reinterpret_cast<volatile int*>(callOffset);
		Write<int>(target - nextInstruction, {callOffset}); // overwrites offset

		return nextInstruction + originalOffset;
	}


	[[nodiscard]] inline address MakeCallHook
	(
		const address     call,
		const void* const target
	) {
		return MakeCallHook(call, reinterpret_cast<address>(target));
	}
}