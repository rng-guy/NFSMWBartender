#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"



namespace DestructionStrings
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves 
	HashContainers::CachedCopyVaultMap<binary> copTypeToDestructionKey(0x0);


	


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
			call HashContainers::CachedCopyVaultMap<binary>::GetValue
			test eax, eax
			je skip                     // string invalid

			push eax

			jmp dword ptr copDestructionExit

			skip:
			jmp dword ptr copDestructionSkip
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [DST] DestructionStrings");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Binary labels
		std::vector<std::string> copVehicles;
		std::vector<std::string> binaryLabels;

		parser.ParseUser<std::string>("Vehicles:Strings", copVehicles, {binaryLabels});

		// Populate label map
		const auto GetBinaryString = reinterpret_cast<const char* (__fastcall*)(int, binary)>(0x56BB80);

		const bool mapIsValid = copTypeToDestructionKey.FillFromVectors<std::string, std::string>
		(
			"Vehicle-to-label",
			HeatParameters::defaultValueHandle,
			copVehicles,
			Globals::StringToVaultKey,
			Globals::DoesVehicleExist,
			binaryLabels,
			Globals::StringToBinaryKey,
			[=](const binary key) -> bool {return GetBinaryString(0, key);}
		);

		if (not mapIsValid) return false;
	
		// Code modifications 
		MemoryTools::MakeRangeJMP(CopDestruction, copDestructionEntrance, copDestructionExit);

		// Status flag
		featureEnabled = true;

		return true;
	}
}