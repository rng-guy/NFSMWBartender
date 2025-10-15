#pragma once

#include <unordered_map>
#include <string>

#include "Globals.h"
#include "HashContainers.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace RadioCallsigns
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves
	enum Group // C-style for inline ASM support
	{
		UNKNOWN,
		PATROL,
		ELITE,
		RHINO,
		CROSS
	};

	Group defaultCallsignGroup = Group::PATROL;

	HashContainers::VaultMap<Group> copTypeToCallsignGroup;

	
	


	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	Group __fastcall GetCallsignGroup(const vault copType)
	{
		const auto foundType = copTypeToCallsignGroup.find(copType);
		return (foundType != copTypeToCallsignGroup.end()) ? foundType->second : defaultCallsignGroup;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address crossCallsignEntrance = 0x71FB01;
	constexpr address crossCallsignExit     = 0x71FB06;

	__declspec(naked) void CrossCallsign()
	{
		__asm
		{
			mov ecx, eax
			call GetCallsignGroup // ecx: copType
			mov dword ptr [esp + 0x28], eax
			cmp eax, CROSS

			jmp dword ptr crossCallsignExit
		}
	}



	constexpr address firstCallsignEntrance = 0x71FB27;
	constexpr address firstCallsignExit     = 0x71FB76;

	__declspec(naked) void FirstCallsign()
	{
		static constexpr address firstCallsignSkip = 0x71FB8B;

		__asm
		{
			cmp dword ptr [esp + 0x28], PATROL
			je conclusion // is "patrol"

			mov eax, 0x20
			cmp dword ptr [esp + 0x28], RHINO
			je skip // is "rhino"

			mov eax, 0x10

			skip:
			jmp dword ptr firstCallsignSkip

			conclusion:
			jmp dword ptr firstCallsignExit
		}
	}



	constexpr address secondCallsignEntrance = 0x71FCCD;
	constexpr address secondCallsignExit     = 0x71FCDD;

	__declspec(naked) void SecondCallsign()
	{
		__asm
		{
			mov eax, -0x1
			cmp dword ptr [esp + 0x28], PATROL
			je conclusion // is "patrol"

			mov eax, 0x20
			cmp dword ptr [esp + 0x28], RHINO
			je conclusion // is "rhino"

			mov eax, 0x10

			conclusion:
			jmp dword ptr secondCallsignExit
		}
	}



	constexpr address collisionCalloutEntrance = 0x71C073;
	constexpr address collisionCalloutExit     = 0x71C08D;

	__declspec(naked) void CollisionCallout()
	{
		__asm
		{
			mov ecx, eax
			call GetCallsignGroup // ecx: copType
			cmp eax, RHINO
			sete byte ptr [esp + 0x2B]

			jmp dword ptr collisionCalloutExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		// Callsign groups
		const std::unordered_map<std::string, Group> nameToCallsign =
		{ 
			{"patrol", Group::PATROL},
			{"elite",  Group::ELITE},
			{"rhino",  Group::RHINO},
			{"cross",  Group::CROSS}
		};

		std::vector<std::string> copVehicles;
		std::vector<std::string> callsignNames;

		const size_t numCopVehicles = parser.ParseUserParameter("Vehicles:Callsigns", copVehicles, callsignNames);

		if (numCopVehicles == 0) return false;

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
		{
			const auto  foundName = nameToCallsign.find(callsignNames[vehicleID]);
			const Group group     = (foundName != nameToCallsign.end()) ? foundName->second : Group::UNKNOWN;

			copTypeToCallsignGroup.insert({Globals::GetVaultKey(copVehicles[vehicleID].c_str()), group});
		}

		// Code caves
		MemoryEditor::Write<byte>(0x24, {0x71FC00, 0x71FC04}); // free up superfluous stack variable

		MemoryEditor::DigCodeCave(CrossCallsign,    crossCallsignEntrance,    crossCallsignExit);
		MemoryEditor::DigCodeCave(FirstCallsign,    firstCallsignEntrance,    firstCallsignExit);
		MemoryEditor::DigCodeCave(SecondCallsign,   secondCallsignEntrance,   secondCallsignExit);
		MemoryEditor::DigCodeCave(CollisionCallout, collisionCalloutEntrance, collisionCalloutExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;
	
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RAD] RadioCallsigns");

		HashContainers::ValidateVaultMap<Group>
		(
			"Vehicle-to-callsign",
			copTypeToCallsignGroup, 
			defaultCallsignGroup,
			[=](const vault key)  {return Globals::VehicleClassMatches(key, Globals::VehicleClass::CAR);},
			[=](const Group value){return (value != Group::UNKNOWN);}
		);
	}
}