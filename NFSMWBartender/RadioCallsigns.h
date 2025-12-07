#pragma once

#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"



namespace RadioCallsigns
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves 
	enum Group // C-style for inline ASM
	{
		UNKNOWN,
		PATROL,
		ELITE,
		RHINO,
		CROSS
	};

	HashContainers::CachedMap<vault, Group> copTypeToCallsignGroup(Group::PATROL);

	
	


	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address crossCallsignEntrance = 0x71FB01;
	constexpr address crossCallsignExit     = 0x71FB06;

	__declspec(naked) void CrossCallsign()
	{
		__asm
		{
			push eax // copType
			mov ecx, offset copTypeToCallsignGroup
			call HashContainers::CachedMap<vault, Group>::GetValue
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
			mov edx, dword ptr [esp + 0x28]

			cmp edx, PATROL
			je conclusion // is "patrol"

			mov eax, 0x20
			cmp edx, RHINO
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
			mov edx, dword ptr [esp + 0x28]

			mov eax, -0x1
			cmp edx, PATROL
			je conclusion // is "patrol"

			mov eax, 0x20
			cmp edx, RHINO
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
			push eax                   // copType
			mov ecx, offset copTypeToCallsignGroup
			call HashContainers::CachedMap<vault, Group>::GetValue
			cmp eax, RHINO
			sete byte ptr [esp + 0x2B] // is "rhino"

			jmp dword ptr collisionCalloutExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		// Callsign groups
		const HashContainers::Map<std::string, Group> nameToCallsign =
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

			copTypeToCallsignGroup.try_emplace(Globals::GetVaultKey(copVehicles[vehicleID].c_str()), group);
		}

		// Code modifications 
		MemoryTools::Write<byte>(0x24, {0x71FC00, 0x71FC04}); // free up superfluous stack variable

		MemoryTools::MakeRangeJMP(CrossCallsign,    crossCallsignEntrance,    crossCallsignExit);
		MemoryTools::MakeRangeJMP(FirstCallsign,    firstCallsignEntrance,    firstCallsignExit);
		MemoryTools::MakeRangeJMP(SecondCallsign,   secondCallsignEntrance,   secondCallsignExit);
		MemoryTools::MakeRangeJMP(CollisionCallout, collisionCalloutEntrance, collisionCalloutExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;
	
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RAD] RadioCallsigns");

		copTypeToCallsignGroup.Validate
		(
			"Vehicle-to-callsign",
			[=](const vault key)   {return Globals::VehicleClassMatches(key, Globals::Class::CAR);},
			[=](const Group value) {return (value != Group::UNKNOWN);}
		);
	}
}