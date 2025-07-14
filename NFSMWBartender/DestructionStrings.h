#pragma once

#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace DestructionStrings 
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	std::unordered_map<hash, key> stringHashToKey;

	const char* const defaultVehicle = "default";
	hash              defaultHash    = 0x0;
	key               defaultKey     = 0x0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	key __fastcall GetBinaryKey(const hash stringHash)
	{
		const auto foundHash = stringHashToKey.find(stringHash);
		return (foundHash == stringHashToKey.end()) ? defaultKey : foundHash->second;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address copDestructionEntrance = 0x595B0D;
	const address copDestructionExit     = 0x595C37;
	const address copDestructionSkip     = 0x595CB3;

	// Looks up Binary key of destroyed vehicle
	__declspec(naked) void CopDestruction()
	{
		__asm
		{
			mov ecx, [esp + 0x54]
			call GetBinaryKey // ecx: stringHash
			test eax, eax
			je skip           // hash is unknown

			push eax

			jmp dword ptr copDestructionExit

			skip:
			jmp dword ptr copDestructionSkip
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		static hash (__cdecl* const GetStringHash)(const char*) = (hash (__cdecl*)(const char*))0x5CC240;
		static key (__cdecl* const GetBinaryKey)(const char*)   = (key (__cdecl*)(const char*))0x460BF0;

		if (not parser.LoadFile(Globals::configPathBasic + "Labels.ini")) return false;

		std::vector<std::string> copVehicles;
		std::vector<std::string> binaryLabels;

		const size_t numCopVehicles = parser.ParseUserParameter("Cops:BinaryStrings", copVehicles, binaryLabels);

		if (not (featureEnabled = (numCopVehicles > 0))) return false;

		defaultHash = GetStringHash(defaultVehicle);

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; vehicleID++)
			stringHashToKey.insert
			(
				{ 
					GetStringHash(copVehicles[vehicleID].c_str()), 
					GetBinaryKey(binaryLabels[vehicleID].c_str()) 
				}
			);
	
		MemoryEditor::DigCodeCave(&CopDestruction, copDestructionEntrance, copDestructionExit);

		return true;
	}



	void VerifyBinaryKeys()
	{
		if (not featureEnabled) return;

		// Can't be run on startup as the game would still be loading assets
		static const char* (__fastcall* const GetString)(int, key) = (const char* (__fastcall*)(int, key))0x56BB80;
		std::erase_if(stringHashToKey, [](const auto& pair) {return (not GetString(0, pair.second));});

		// Extract "default" key if provided (and valid)
		const auto pair = stringHashToKey.extract(defaultHash);
		if (not pair.empty()) defaultKey = pair.mapped();
	}
}