#pragma once

#include <vector>
#include <string>
#include <algorithm>

#include "Globals.h"
#include "MemoryEditor.h"
#include "HeatParameters.h"



namespace DestructionStrings
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves
	binary                    defaultDestructionKey;
	Globals::VaultMap<binary> copTypeToDestructionKey;


	


	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	binary __fastcall GetDestructionKey(const vault copType)
	{
		const auto foundType = copTypeToDestructionKey.find(copType);
		return (foundType != copTypeToDestructionKey.end()) ? foundType->second : defaultDestructionKey;
	}



	bool IsValidBinaryKey(const binary key)
	{
		static const char* (__fastcall* const GetBinaryString)(int, binary) = (const char* (__fastcall*)(int, binary))0x56BB80;

		return GetBinaryString(0, key);
	}



	bool IsNotValidPair(const std::pair<const vault, binary>& pair)
	{ 
		return (not (HeatParameters::IsValidVehicle(pair.first) and IsValidBinaryKey(pair.second)));
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copDestructionEntrance = 0x595B0D;
	constexpr address copDestructionExit     = 0x595C37;

	// Looks up Binary key of destroyed vehicle
	__declspec(naked) void CopDestruction()
	{
		static constexpr address copDestructionSkip = 0x595CB3;

		__asm
		{
			mov ecx, dword ptr [esp + 0x54]
			call GetDestructionKey // ecx: copType
			test eax, eax
			je skip                // type unknown and no default key

			push eax

			jmp dword ptr copDestructionExit

			skip:
			jmp dword ptr copDestructionSkip
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Labels.ini")) return false;

		std::vector<std::string> copVehicles;
		std::vector<std::string> binaryLabels;

		const size_t numCopVehicles = parser.ParseUserParameter("Cops:BinaryStrings", copVehicles, binaryLabels);
		if (not (featureEnabled = (numCopVehicles > 0))) return false;

		static binary (__cdecl* const GetBinaryKey)(const char*) = (binary (__cdecl*)(const char*))0x460BF0;

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; vehicleID++)
		{
			copTypeToDestructionKey.insert
			(
				{ 
					Globals::GetVaultKey(copVehicles[vehicleID].c_str()),
					GetBinaryKey(binaryLabels[vehicleID].c_str()) 
				}
			);
		}
	
		MemoryEditor::DigCodeCave(CopDestruction, copDestructionEntrance, copDestructionExit);

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  Config [DST] DestructionStrings");

		// Extract "default" key if provided (and valid)
		const auto pair       = copTypeToDestructionKey.extract(Globals::GetVaultKey("default"));
		const bool isValid    = (not pair.empty()) and IsValidBinaryKey(pair.mapped());
		defaultDestructionKey = (isValid) ? pair.mapped() : 0x0;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.LogLongIndent((isValid) ? "Valid" : "No valid", "default key");

		// Remove any invalid pairs
		const size_t numInvalidPairs = std::erase_if(copTypeToDestructionKey, IsNotValidPair);

		if constexpr (Globals::loggingEnabled)
			if (numInvalidPairs > 0) Globals::logger.LogLongIndent("Removed", (int)numInvalidPairs, "invalid pairs");
	}
}