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
	key                   defaultDestructionKey;
	Globals::HashMap<key> copTypeToDestructionKey;


	


	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	key __fastcall GetDestructionKey(const hash copType)
	{
		const auto foundType = copTypeToDestructionKey.find(copType);
		return (foundType != copTypeToDestructionKey.end()) ? foundType->second : defaultDestructionKey;
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

		static key (__cdecl* const GetBinaryKey)(const char*) = (key (__cdecl*)(const char*))0x460BF0;

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; vehicleID++)
			copTypeToDestructionKey.insert({Globals::GetStringHash(copVehicles[vehicleID].c_str()), GetBinaryKey(binaryLabels[vehicleID].c_str())});
	
		MemoryEditor::DigCodeCave(CopDestruction, copDestructionEntrance, copDestructionExit);

		return true;
	}



	void VerifyBinaryKeys()
	{
		if (not featureEnabled) return;

		// Can't be run on startup as the game would still be loading assets
		static const char* (__fastcall* const GetBinaryString)(int, key) = (const char* (__fastcall*)(int, key))0x56BB80;
		std::erase_if(copTypeToDestructionKey, [](const auto& pair) {return (not GetBinaryString(0, pair.second));});

		// Extract "default" key if provided (and valid)
		const auto pair       = copTypeToDestructionKey.extract(Globals::GetStringHash("default"));
		defaultDestructionKey = (not pair.empty()) ? pair.mapped() : 0x0;
	}
}