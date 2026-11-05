#pragma once

#include <string>
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
	[[nodiscard]] inline address AsAddress(const T* const target) noexcept
	{
		return reinterpret_cast<address>(target);
	}


	template <typename T>
	requires std::is_function_v<T>
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
		const address base = AsAddress(GetModuleHandleA(nullptr));

		const auto* const dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
		const auto* const nt  = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);

		return nt->OptionalHeader.AddressOfEntryPoint;
	}





	// Direct writing -------------------------------------------------------------------------------------------------------------------------------

	template <class Patcher>
	requires std::invocable<Patcher&, void*, size_t>
	inline void Apply
	(
		const address target,
		const size_t  numBytes,
		Patcher&&     TargetPatch
	) {
		DWORD       setting  = PAGE_EXECUTE_READWRITE;
		void* const location = AsPointer<void>(target);

		VirtualProtect(location, numBytes, setting, &setting);
		TargetPatch   (location, numBytes);
		VirtualProtect(location, numBytes, setting, &setting);

		FlushInstructionCache(GetCurrentProcess(), location, numBytes);
	}



	template <typename T>
	requires std::is_trivially_copyable_v<T>
	inline void Write
	(
		const T&                             data,
		const std::initializer_list<address> targets
	) {
		for (const address target : targets)
		{
			const auto TargetPatch = [&data](void* const location, const size_t numBytes) -> void
			{
				std::memcpy(location, &data, numBytes);
			};

			Apply(target, sizeof(T), TargetPatch);
		}
	}





	// Range-writing helpers ------------------------------------------------------------------------------------------------------------------------

	namespace Details
	{
		inline void WriteToRange
		(
			const byte    value,
			const address begin,
			const address end
		) {
			const auto TargetPatch = [value](void* const location, const size_t numBytes) -> void
			{
				std::memset(location, value, numBytes);
			};

			Apply(begin, end - begin, TargetPatch);
		}



		inline void MakeRangeNOP
		(
			const address begin,
			const address end
		) {
			WriteToRange(/* value = */ 0x90, begin, end); // NOP
		}



		inline void MakeRangeJMP
		(
			const address target,
			const address begin,
			const address end
		) {
			MakeRangeNOP(begin, end);

			const address jumpTargetOffset = begin            + sizeof(byte);
			const address nextInstruction  = jumpTargetOffset + sizeof(ptrdiff_t);

			Write<byte>     (/* value = */ 0xE9,       {begin}); // jump near, relative
			Write<ptrdiff_t>(target - nextInstruction, {jumpTargetOffset});
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
	inline void WriteToRange(const byte value)
	{
		static_assert(end > begin, "Invalid write range");

		Details::WriteToRange(value, begin, end);
	}



	template <address begin, address end>
	inline void MakeRangeNOP()
	{
		static_assert(end > begin, "Invalid NOP range");

		Details::MakeRangeNOP(begin, end);
	}



	template <address begin, address end>
	inline void MakeRangeJMP(const address target)
	{
		static_assert(end >= begin + sizeof(byte) + sizeof(ptrdiff_t), "Invalid JMP range");

		Details::MakeRangeJMP(target, begin, end);
	}


	template <address begin, address end, typename T>
	requires std::is_function_v<T>
	inline void MakeRangeJMP(T* const target)
	{
		MakeRangeJMP<begin, end>(AsAddress<T>(target));
	}
	




	// Function-hooking abstraction macros ----------------------------------------------------------------------------------------------------------

	#define MT_DETAILS_ORIGINAL(name) name##Original



	#define HOOK_ORIGINAL(name) MemoryTools::address MT_DETAILS_ORIGINAL(name) = 0x0 // declare pointer to replaced function

	#define CALL_HOOK_ORIGINAL(name, ...) MemoryTools::AsFunction<decltype(name)>(MT_DETAILS_ORIGINAL(name))(__VA_ARGS__) // call replaced function

	#define PATCH_HOOK_FUNCTION(name, target) MT_DETAILS_ORIGINAL(name) = MemoryTools::ReplaceCALL(name, target) // apply hook





	// Function hooking -----------------------------------------------------------------------------------------------------------------------------

	inline address ReplaceCALL
	(
		const address newTarget,
		const address callSite
	) {
		const address callOffset      = callSite   + sizeof(byte);
		const address nextInstruction = callOffset + sizeof(ptrdiff_t);

		const ptrdiff_t originalOffset = AsReference<ptrdiff_t>(callOffset);
		Write<ptrdiff_t>(newTarget - nextInstruction, {callOffset});

		return nextInstruction + originalOffset; // replaced target
	}



	template <typename T>
	requires std::is_function_v<T>
	inline address ReplaceCALL
	(
		T* const      newTarget,
		const address callSite
	) {
		return ReplaceCALL(AsAddress<T>(newTarget), callSite);
	}
}