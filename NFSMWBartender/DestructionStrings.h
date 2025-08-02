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

	std::unordered_map<hash, key> copTypeToDestructionKey;

	hash defaultType = 0x0;
	key  defaultKey  = 0x0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	key __fastcall GetDestructionKey(const hash copType)
	{
		const auto foundType = copTypeToDestructionKey.find(copType);
		return (foundType != copTypeToDestructionKey.end()) ? foundType->second : defaultKey;
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
			je skip                // type unknown and no valid default key

			push eax

			jmp dword ptr copDestructionExit

			skip:
			jmp dword ptr copDestructionSkip
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if (not parser.LoadFile(Globals::configPathBasic + "Labels.ini")) return false;

		static hash (__cdecl* const GetStringHash)(const char*) = (hash (__cdecl*)(const char*))0x5CC240;
		static key (__cdecl* const GetBinaryKey)(const char*)   = (key (__cdecl*)(const char*))0x460BF0;

		std::vector<std::string> copVehicles;
		std::vector<std::string> binaryLabels;

		const size_t numCopVehicles = parser.ParseUserParameter("Cops:BinaryStrings", copVehicles, binaryLabels);

		if (not (featureEnabled = (numCopVehicles > 0))) return false;

		defaultType = GetStringHash("default");

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; vehicleID++)
			copTypeToDestructionKey.insert({GetStringHash(copVehicles[vehicleID].c_str()), GetBinaryKey(binaryLabels[vehicleID].c_str())});
	
		MemoryEditor::DigCodeCave(&CopDestruction, copDestructionEntrance, copDestructionExit);

		return true;
	}



	void VerifyBinaryKeys()
	{
		if (not featureEnabled) return;

		// Can't be run on startup as the game would still be loading assets
		static const char* (__fastcall* const GetString)(int, key) = (const char* (__fastcall*)(int, key))0x56BB80;
		std::erase_if(copTypeToDestructionKey, [](const auto& pair) {return (not GetString(0, pair.second));});

		// Extract "default" key if provided (and valid)
		const auto pair = copTypeToDestructionKey.extract(defaultType);
		if (not pair.empty()) defaultKey = pair.mapped();
	}
}