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
	HashContainers::CachedVaultMap<binary> copTypeToDestructionKey(0x0);


	


	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copDestructionEntrance = 0x595B0D;
	constexpr address copDestructionExit     = 0x595C37;

	// Looks up Binary key of destroyed vehicle
	__declspec(naked) void CopDestruction()
	{
		static constexpr address copDestructionSkip = 0x595CB3;

		__asm
		{
			push dword ptr [esp + 0x54] // copType
			mov ecx, offset copTypeToDestructionKey
			call HashContainers::CachedVaultMap<binary>::GetValue
			test eax, eax
			je skip                     // type unknown and no default key

			push eax

			jmp dword ptr copDestructionExit

			skip:
			jmp dword ptr copDestructionSkip
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		// Binary labels
		std::vector<std::string> copVehicles;
		std::vector<std::string> binaryLabels;

		const size_t numCopVehicles = parser.ParseUserParameter("Vehicles:Strings", copVehicles, binaryLabels);

		if (numCopVehicles == 0) return false;

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
		{
			copTypeToDestructionKey.try_emplace
			(
				Globals::GetVaultKey (copVehicles [vehicleID].c_str()), 
				Globals::GetBinaryKey(binaryLabels[vehicleID].c_str())
			);
		}
	
		// Code modifications 
		MemoryTools::MakeRangeJMP(CopDestruction, copDestructionEntrance, copDestructionExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [DST] DestructionStrings");

		static const auto GetBinaryString = reinterpret_cast<const char* (__fastcall*)(int, binary)>(0x56BB80);

		copTypeToDestructionKey.Validate
		(
			"Vehicle-to-label",
			[=](const vault  key)   {return Globals::VehicleClassMatches(key, Globals::Class::ANY);},
			[=](const binary value) {return GetBinaryString(0, value);}
		);
	}
}