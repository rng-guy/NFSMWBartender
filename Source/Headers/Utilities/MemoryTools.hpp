#pragma once

#include <string>
#include <cassert>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <concepts>
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
	requires (std::is_object_v<T> or std::is_void_v<T>)
	[[nodiscard]] inline address AsAddress(const T* const object) noexcept
	{
		return reinterpret_cast<address>(object);
	}


	template <typename T>
	requires std::is_function_v<T>
	[[nodiscard]] inline address AsAddress(T* const function) noexcept
	{
		return reinterpret_cast<address>(function);
	}



	template <typename T>
	requires (std::is_object_v<T> or std::is_void_v<T>)
	[[nodiscard]] inline T* AsPointer(const address object) noexcept
	{
		return reinterpret_cast<T*>(object);
	}


	template <typename T>
	requires std::is_object_v<T>
	[[nodiscard]] inline T& AsReference(const address object) noexcept
	{
		return *AsPointer<T>(object);
	}



	template <typename T>
	requires std::is_function_v<T>
	[[nodiscard]] inline T* AsFunction(const address function) noexcept
	{
		return reinterpret_cast<T*>(function);
	}





	// Module queries -------------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] inline bool IsModuleLoaded(const char* const moduleName)
	{
		return GetModuleHandleA(moduleName);
	}


	[[nodiscard]] inline bool IsModuleLoaded(const std::string& moduleName)
	{
		return IsModuleLoaded(moduleName.c_str());
	}



	[[nodiscard]] inline address GetEntryPoint()
	{
		const address base = AsAddress(GetModuleHandleA(nullptr));

		const auto* const dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
		const auto* const nt  = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);

		return nt->OptionalHeader.AddressOfEntryPoint;
	}





	// General writing ------------------------------------------------------------------------------------------------------------------------------

	template <class Patcher>
	requires std::invocable<Patcher&, address, size_t>
	inline void ApplyPatch
	(
		Patcher&&     Patch,
		const address target,
		const size_t  numBytes
	) {
		DWORD       setting  = PAGE_EXECUTE_READWRITE;
		void* const location = AsPointer<void>(target);

		if (VirtualProtect(location, numBytes, setting, &setting))
		{
			Patch(target, numBytes);

			if (not VirtualProtect(location, numBytes, setting, &setting)) assert(not "restored");

			FlushInstructionCache(GetCurrentProcess(), location, numBytes);
		}
		else assert(not "unprotected");
	}



	template <typename T>
	requires std::is_trivially_copyable_v<T>
	inline void WriteToAddresses
	(
		const T&                             source,
		const std::initializer_list<address> destinations
	) {
		const auto PatchCopy = [&source](const address target, const size_t numBytes) -> void
		{
			std::memcpy(AsPointer<void>(target), &source, numBytes);
		};

		for (const address destination : destinations)
			ApplyPatch(PatchCopy, destination, sizeof(T));
	}





	// Range-writing helpers ------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{
		inline void PatchToNOP
		(
			const address target,
			const size_t  numBytes
		) {
			std::memset(AsPointer<void>(target), 0x90, numBytes); // NOP
		}



		inline void MakeRangeNOP
		(
			const address begin,
			const address end
		) {
			ApplyPatch(PatchToNOP, begin, end - begin);
		}



		inline void MakeRangeJMP
		(
			const address jmpTarget,
			const address begin,
			const address end
		) {
			const auto PatchJMP = [jmpTarget](const address target, const size_t numBytes) -> void
			{
				PatchToNOP(target, numBytes);

				constexpr byte opcode = 0xE9; // jump near, relative

				const address   jmpOperand = target + sizeof(byte);
				const ptrdiff_t jmpOffset  = jmpTarget - (jmpOperand + sizeof(ptrdiff_t));

				std::memcpy(AsPointer<void>(target),     &opcode,    sizeof(byte));
				std::memcpy(AsPointer<void>(jmpOperand), &jmpOffset, sizeof(ptrdiff_t));
			};

			ApplyPatch(PatchJMP, begin, end - begin);
		}
	}





	// Assembly-detouring abstraction macros --------------------------------------------------------------------------------------------------------

	#define MT_DETAILS_BEGIN(name) asm##name##Begin

	#define MT_DETAILS_END(name) asm##name##End

	#define MT_DETAILS_ADDRESS(variable, value) constexpr MemoryTools::address variable = value

	#define MT_DETAILS_RANGE(name, begin, end) MT_DETAILS_ADDRESS(MT_DETAILS_BEGIN(name), begin); MT_DETAILS_ADDRESS(MT_DETAILS_END(name), end)



	#define ASSEMBLY_DETOUR(name, begin, end) MT_DETAILS_RANGE(name, begin, end); void __declspec(naked) name() // declare detour

	#define EXIT_ASSEMBLY_DETOUR(name) jmp dword ptr [MT_DETAILS_END(name)] // jump to detour exit in ASM

	#define PATCH_ASSEMBLY_DETOUR(name) MemoryTools::MakeRangeJMP<MT_DETAILS_BEGIN(name), MT_DETAILS_END(name)>(name) // apply detour





	// Address-range writing ------------------------------------------------------------------------------------------------------------------------

	template <address begin, address end>
	inline void MakeRangeNOP()
	{
		static_assert(end > begin, "Range must cover at least 1 byte");

		Details::MakeRangeNOP(begin, end);
	}



	template <address begin, address end>
	inline void MakeRangeJMP(const address jmpTarget)
	{
		static_assert(end >= begin + sizeof(byte) + sizeof(ptrdiff_t), "Range must cover length of JMP instruction");

		Details::MakeRangeJMP(jmpTarget, begin, end);
	}


	template <address begin, address end, typename T>
	requires std::is_function_v<T>
	inline void MakeRangeJMP(T* const jmpTarget)
	{
		MakeRangeJMP<begin, end>(AsAddress<T>(jmpTarget));
	}
	




	// Function-hooking abstraction macros ----------------------------------------------------------------------------------------------------------

	#define MT_DETAILS_ORIGINAL(name) name##Original



	#define HOOK_ORIGINAL(name) MemoryTools::address MT_DETAILS_ORIGINAL(name) = 0x0 // declare pointer to replaced function

	#define CALL_HOOK_ORIGINAL(name, ...) MemoryTools::AsFunction<decltype(name)>(MT_DETAILS_ORIGINAL(name))(__VA_ARGS__) // call replaced function

	#define PATCH_HOOK_FUNCTION(name, target) MT_DETAILS_ORIGINAL(name) = MemoryTools::ReplaceCALL(name, target) // apply hook





	// Function hooking -----------------------------------------------------------------------------------------------------------------------------

	inline address ReplaceCALL
	(
		const address newCallTarget,
		const address callSite
	) {
		const address callOperand = callSite    + sizeof(byte);
		const address callEnd     = callOperand + sizeof(ptrdiff_t);

		const ptrdiff_t oldOperand = AsReference<ptrdiff_t>(callOperand);
		WriteToAddresses<ptrdiff_t>(newCallTarget - callEnd, {callOperand});

		return callEnd + oldOperand; // replaced target
	}



	template <typename T>
	requires std::is_function_v<T>
	inline address ReplaceCALL
	(
		T* const      newCallTarget,
		const address callSite
	) {
		return ReplaceCALL(AsAddress<T>(newCallTarget), callSite);
	}
}