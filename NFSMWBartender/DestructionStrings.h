#pragma once

#include <vector>
#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace DestructionStrings
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves 
	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<std::string> copTypeToNotification(std::string{});
	




	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	const char* __fastcall GetDestructionString(const vault copType)
	{
		return copTypeToNotification.GetReference(copType).c_str();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copDestructionEntrance = 0x595B0D;
	constexpr address copDestructionExit     = 0x595C41;

	// Looks up notification strings for destroyed cop vehicles
	__declspec(naked) void CopDestruction()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x54]
			call GetDestructionString // ecx: copType
			cmp byte ptr [eax], '\0'

			jmp dword ptr copDestructionExit
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseDestructionKeys(const HeatParameters::Parser& parser)
	{
		std::vector<const char*> copVehicles;   // C-style for game compatibility
		std::vector<const char*> stringOrNames; // C-style for game compatibility

		parser.ParseUser<const char*, const char*>("Vehicles:Notifications", copVehicles, {stringOrNames});

		const auto StringOrNameToNotification = [](const char* const stringOrName) -> std::string
		{
			const auto        GetBinaryString = reinterpret_cast<const char* (__fastcall*)(int, binary)>(0x56BB80);
			const char* const binaryString    = GetBinaryString(0, Globals::GetBinaryKey(stringOrName));

			return (binaryString) ? binaryString : stringOrName;
		};

		return copTypeToNotification.FillFromVectors
		(
			"Vehicle-to-notification",
			Globals::GetVaultKey(HeatParameters::configDefaultKey),
			ModContainers::MapFillSetup(copVehicles,   Globals::GetVaultKey,       Globals::DoesVehicleTypeExist),
			ModContainers::MapFillSetup(stringOrNames, StringOrNameToNotification, ModContainers::AlwaysValid{})
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [DST] DestructionStrings");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Destruction strings
		if (not ParseDestructionKeys(parser)) return false; // no valid string(s); disable feature
	
		// Code modifications 
		MemoryTools::MakeRangeJMP<copDestructionEntrance, copDestructionExit>(CopDestruction);

		// Status flag
		featureEnabled = true;

		return true;
	}
}