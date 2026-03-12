#pragma once

#include <vector>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace DestructionStrings
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves 
	constinit ModContainers::DefaultCopyVaultMap<binary> copTypeToDestructionKey(0x0);


	


	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copDestructionEntrance = 0x595B0D;
	constexpr address copDestructionExit     = 0x595C37;

	// Looks up notification strings for destroyed cop vehicles
	__declspec(naked) void CopDestruction()
	{
		static constexpr address copDestructionSkip = 0x595CB3;

		__asm
		{
			push dword ptr [esp + 0x54] // copType
			mov ecx, offset copTypeToDestructionKey
			call ModContainers::DefaultCopyVaultMap<binary>::GetValue
			test eax, eax
			je skip                     // string invalid

			push eax

			jmp dword ptr copDestructionExit

			skip:
			jmp dword ptr copDestructionSkip
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseDestructionKeys(const HeatParameters::Parser& parser)
	{
		std::vector<const char*> copVehicles;  // for game compatibility
		std::vector<const char*> binaryLabels; // for game compatibility

		parser.ParseUser<const char*, const char*>("Vehicles:Strings", copVehicles, {binaryLabels});

		const auto IsBinaryKeyValid = [](const binary key) -> bool
		{
			const auto GetBinaryString = reinterpret_cast<const char* (__fastcall*)(int, binary)>(0x56BB80);
			return GetBinaryString(0, key); // first argument is unused placeholder value
		};

		return copTypeToDestructionKey.FillFromVectors
		(
			"Vehicle-to-label",
			Globals::GetVaultKey(HeatParameters::configDefaultKey),
			ModContainers::FillSetup(copVehicles,  Globals::GetVaultKey,  Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(binaryLabels, Globals::GetBinaryKey, IsBinaryKeyValid)
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [DST] DestructionStrings");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Destruction (binary) keys
		if (not ParseDestructionKeys(parser)) return false; // no valid keys; disable feature
	
		// Code modifications 
		MemoryTools::MakeRangeJMP(CopDestruction, copDestructionEntrance, copDestructionExit);

		// Status flag
		featureEnabled = true;

		return true;
	}
}