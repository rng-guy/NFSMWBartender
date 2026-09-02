#pragma once

#include <string>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <type_traits>
#include <initializer_list>

#include <Windows.h>
#include <memoryapi.h>
#include <libloaderapi.h>
#include <processthreadsapi.h>



namespace MemoryTools
{
	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using byte = uint8_t;
	using word = uint16_t;

	using address = uintptr_t;





	// Address casting ------------------------------------------------------------------------------------------------------------------------------

	template <typename T>
	[[nodiscard]] inline address AsAddress(T* const target) noexcept
	{
		return reinterpret_cast<address>(target);
	}



	template <typename T>
	requires (std::is_object_v<T> or std::is_void_v<T>)
	[[nodiscard]] inline T* AsPointer(const address target) noexcept
	{
		return reinterpret_cast<T*>(target);
	}


	template <typename T>
	requires std::is_object_v<T>
	[[nodiscard]] inline T& AsReference(const address target) noexcept
	{
		return *AsPointer<T>(target);
	}



	template <typename T>
	requires std::is_function_v<T>
	[[nodiscard]] inline T* AsFunction(const address target) noexcept
	{
		return reinterpret_cast<T*>(target);
	}





	// Module queries -------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] inline bool IsModuleLoaded(const char* const name)
	{
		return GetModuleHandleA(name);
	}


	[[nodiscard]] inline bool IsModuleLoaded(const std::string& name)
	{
		return IsModuleLoaded(name.c_str());
	}



	[[nodiscard]] inline address GetEntryPoint()
	{
		// Credit: thelink2012 and MWisBest
		const address base = AsAddress(GetModuleHandleA(NULL));

		const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
		const auto nt  = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);

		return nt->OptionalHeader.AddressOfEntryPoint;
	}





	// Direct-address writing -----------------------------------------------------------------------------------------------------------------------

	template <typename T>
	requires std::is_trivially_copyable_v<T>
	inline void Write
	(
		const T                              data,
		const std::initializer_list<address> targets
	) {
		constexpr size_t numBytes = sizeof(T);

		for (const address target : targets)
		{
			DWORD previousSetting = PAGE_READONLY; // arbitrary
			void* memoryLocation  = AsPointer<void>(target);

			VirtualProtect(memoryLocation, numBytes, PAGE_EXECUTE_READWRITE, &previousSetting);
			std::memcpy   (memoryLocation, &data,    numBytes);
			VirtualProtect(memoryLocation, numBytes, previousSetting,        &previousSetting);

			FlushInstructionCache(GetCurrentProcess(), memoryLocation, numBytes);
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

			DWORD previousSetting = PAGE_READONLY; // arbitrary
			void* memoryLocation  = AsPointer<void>(start);

			VirtualProtect(memoryLocation, numBytes, PAGE_EXECUTE_READWRITE, &previousSetting);
			std::memset   (memoryLocation, value,    numBytes);
			VirtualProtect(memoryLocation, numBytes, previousSetting,        &previousSetting);

			FlushInstructionCache(GetCurrentProcess(), memoryLocation, numBytes);
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
			const address nextInstruction  = jumpTargetOffset + sizeof(ptrdiff_t);

			MakeRangeNOP(start, end);

			Write<byte>     (0xE9,                     {start}); // jump near, relative
			Write<ptrdiff_t>(target - nextInstruction, {jumpTargetOffset});
		}
	}





	// Address-range writing ------------------------------------------------------------------------------------------------------------------------

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





	// Assembly-detouring helpers -------------------------------------------------------------------------------------------------------------------

	#define MT_DETAILS_BEGIN(name) asm##name##Begin

	#define MT_DETAILS_END(name) asm##name##End

	#define MT_DETAILS_ADDRESS(variable, value) constexpr MemoryTools::address variable = value

	#define MT_DETAILS_RANGE(name, begin, end) MT_DETAILS_ADDRESS(MT_DETAILS_BEGIN(name), begin); MT_DETAILS_ADDRESS(MT_DETAILS_END(name), end)





	// Assembly detouring ---------------------------------------------------------------------------------------------------------------------------

	#define ASSEMBLY_DETOUR(name, begin, end) MT_DETAILS_RANGE(name, begin, end); __declspec(naked) void name()

	#define EXIT_ASSEMBLY_DETOUR(name) jmp dword ptr [MT_DETAILS_END(name)]

	#define PATCH_ASSEMBLY_DETOUR(name) MemoryTools::MakeRangeJMP<MT_DETAILS_BEGIN(name), MT_DETAILS_END(name)>(name)



	template <address start, address end>
	inline void MakeRangeJMP(const address target)
	{
		static_assert(end >= start + sizeof(byte) + sizeof(ptrdiff_t), "Cannot accommodate JMP");

		Details::MakeRangeJMP(start, end, target);
	}



	template <address start, address end, typename T>
	requires std::is_function_v<T>
	inline void MakeRangeJMP(T* const target)
	{
		MakeRangeJMP<start, end>(AsAddress<T>(target));
	}
	




	// Function-hooking helpers ---------------------------------------------------------------------------------------------------------------------

	#define MT_DETAILS_ORIGINAL(name) name##Original





	// Function hooking -----------------------------------------------------------------------------------------------------------------------------

	#define HOOK_ORIGINAL(name) MemoryTools::address MT_DETAILS_ORIGINAL(name) = 0x0

	#define CALL_HOOK_ORIGINAL(name, ...) MemoryTools::AsFunction<decltype(name)>(MT_DETAILS_ORIGINAL(name))(__VA_ARGS__)

	#define PATCH_HOOK_FUNCTION(name, target) MT_DETAILS_ORIGINAL(name) = MemoryTools::ReplaceCall(target, name)



	inline address ReplaceCall
	(
		const address callSite,
		const address newTarget
	) {
		const address callOffset      = callSite   + sizeof(byte);
		const address nextInstruction = callOffset + sizeof(ptrdiff_t);

		const ptrdiff_t originalOffset = AsReference<ptrdiff_t>(callOffset);
		Write<ptrdiff_t>(newTarget - nextInstruction, {callOffset});

		return nextInstruction + originalOffset; // replaced target
	}



	template <typename T>
	requires std::is_function_v<T>
	inline address ReplaceCall
	(
		const address callSite,
		T* const      newTarget
	) {
		return ReplaceCall(callSite, AsAddress<T>(newTarget));
	}
}