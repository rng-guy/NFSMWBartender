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
	enum Callsign // C-style for inline ASM
	{
		UNKNOWN,
		PATROL,
		ELITE,
		RHINO,
		CROSS
	};

	Callsign defaultCallsign = PATROL;

	HashContainers::VaultMap<Callsign> copTypeToCallsign;

	
	


	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	Callsign __fastcall GetCallsign(const vault copType)
	{
		const auto foundType = copTypeToCallsign.find(copType);
		return (foundType != copTypeToCallsign.end()) ? foundType->second : defaultCallsign;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address crossCallsignEntrance = 0x71FB01;
	constexpr address crossCallsignExit     = 0x71FB06;

	__declspec(naked) void CrossCallsign()
	{
		__asm
		{
			mov ecx, eax
			call GetCallsign // ecx: copType
			cmp eax, CROSS

			jmp dword ptr crossCallsignExit
		}
	}



	constexpr address firstCallsignEntrance = 0x71FB2F;
	constexpr address firstCallsignExit     = 0x71FB76;

	__declspec(naked) void FirstCallsign()
	{
		static constexpr address firstCallsignSkip = 0x71FB8B;

		__asm
		{
			mov ecx, eax
			call GetCallsign // ecx: copType
			cmp eax, PATROL
			je conclusion    // is "patrol"

			cmp eax, RHINO
			mov eax, 0x20
			je skip // is "rhino"

			mov eax, 0x10

			skip:
			jmp dword ptr firstCallsignSkip

			conclusion:
			jmp dword ptr firstCallsignExit
		}
	}



	constexpr address secondCallsignEntrance = 0x701190;
	constexpr address secondCallsignExit     = 0x7011C8;

	__declspec(naked) void SecondCallsign()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x4]
			call GetCallsign // ecx: copType
			mov ecx, eax

			mov eax, -0x1
			cmp ecx, PATROL
			je conclusion // is "patrol"

			mov eax, 0x20
			cmp ecx, RHINO
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
			call GetCallsign // ecx: copType
			cmp eax, RHINO
			sete byte ptr [esp + 0x2B]

			jmp dword ptr collisionCalloutExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		// Callsigns
		const std::unordered_map<std::string, Callsign> nameToCallsign = 
		{ 
			{"patrol", PATROL}, 
			{"elite",  ELITE}, 
			{"rhino",  RHINO}, 
			{"cross",  CROSS} 
		};

		std::vector<std::string> copVehicles;
		std::vector<std::string> callsignNames;

		const size_t numCopVehicles = parser.ParseUserParameter("Vehicles:Callsigns", copVehicles, callsignNames);

		featureEnabled = (numCopVehicles > 0);
		if (not featureEnabled) return false;

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; vehicleID++)
		{
			const auto     foundName = nameToCallsign.find(callsignNames[vehicleID]);
			const Callsign callsign  = (foundName != nameToCallsign.end()) ? foundName->second : UNKNOWN;

			copTypeToCallsign.insert({Globals::GetVaultKey(copVehicles[vehicleID].c_str()), callsign});
		}

		// Code caves
		MemoryEditor::DigCodeCave(CrossCallsign,    crossCallsignEntrance,    crossCallsignExit);
		MemoryEditor::DigCodeCave(FirstCallsign,    firstCallsignEntrance,    firstCallsignExit);
		MemoryEditor::DigCodeCave(SecondCallsign,   secondCallsignEntrance,   secondCallsignExit);
		MemoryEditor::DigCodeCave(CollisionCallout, collisionCalloutEntrance, collisionCalloutExit);

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;
	
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RAD] RadioCallsigns");

		HashContainers::ValidateVaultMap<Callsign>
		(
			"Vehicle-to-callsign",
			copTypeToCallsign, 
			defaultCallsign,
			[=](const vault    key)  {return Globals::VehicleClassMatches(key, false);},
			[=](const Callsign value){return (value != UNKNOWN);}
		);
	}
}