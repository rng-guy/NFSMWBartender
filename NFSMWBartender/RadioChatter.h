#pragma once

#include <vector>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace RadioChatter
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Enums (C-style for ASM)
	enum Jurisdiction
	{
		CITY    = 0,
		STATE   = 1,
		FEDERAL = 2
	};
	
	enum Battalion
	{
		UNKNOWN,
		PATROL,
		ELITE,
		RHINO,
		CROSS
	};

	// Heat parameters
	constinit HeatParameters::Pair<int> heatJurisdictionIDs(Jurisdiction::CITY);

	// Code caves 
	size_t lastReportedHeatLevel = 1;
	int    lastJurisdictionID    = 0;

	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<Battalion> copTypeToBattalion(Battalion::PATROL);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	int __fastcall GetCallsignsOffset(const Battalion battalion)
	{
		switch (battalion)
		{
		case Battalion::PATROL:
			return -0x1;

		case Battalion::RHINO:
			return 0x20;
		}

		return 0x10; // e.g. ELITE, CROSS
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatCheckEntrance = 0x71D370;
	constexpr address heatCheckExit     = 0x71D3AE;

	// Checks whether a new Heat-level announcement is due
	__declspec(naked) void HeatCheck()
	{
		static constexpr address heatCheckSkip = 0x71D433;

		__asm
		{
			mov ebp, dword ptr [esi + 0x104] // current Heat level

			cmp ebp, dword ptr [lastReportedHeatLevel]
			je skip // already reported

			mov dword ptr [lastReportedHeatLevel], ebp

			cmp ebp, 10
			jg skip // new Heat Level > 10

			sub ebp, 2
			jl skip // new Heat level < 2

			jmp dword ptr [heatCheckExit]

			skip:
			jmp dword ptr [heatCheckSkip]
		}
	}



	constexpr address heatReportEntrance = 0x71D428;
	constexpr address heatReportExit     = 0x71D42F;

	// Converts the Heat-level ID into a copspeech offset
	__declspec(naked) void HeatReport()
	{
		__asm
		{
			xchg ecx, ebp // ebp from "HeatCheck"

			mov eax, 1
			shl eax, cl

			mov ecx, ebp

			jmp dword ptr [heatReportExit]
		}
	}



	constexpr address playerPursuitEntrance = 0x704F70;
	constexpr address playerPursuitExit     = 0x704F76;

	// Resets transition state whenever a new player pursuit begins
	__declspec(naked) void PlayerPursuit()
	{
		__asm
		{
			mov dword ptr [lastReportedHeatLevel], 1
			mov dword ptr [lastJurisdictionID], CITY

			// Execute original code and resume
			mov dword ptr [esi + 0x130], edi

			jmp dword ptr [playerPursuitExit]
		}
	}



	constexpr address callsignsCheckEntrance = 0x71FB01;
	constexpr address callsignsCheckExit     = 0x71FB06;

	// Retrieves the current vehicle's callsign
	__declspec(naked) void CallsignsCheck()
	{
		__asm
		{
			push eax // copType
			mov ecx, offset copTypeToBattalion
			call ModContainers::DefaultVaultMap<Battalion>::GetValue
			cmp eax, CROSS

			mov dword ptr [esp + 0x28], eax // freed variable

			jmp dword ptr [callsignsCheckExit]
		}
	}



	constexpr address firstCallsignsEntrance = 0x71FB27;
	constexpr address firstCallsignsExit     = 0x71FB76;

	// The first callsigns-specific part of the assignment function
	__declspec(naked) void FirstCallsigns()
	{
		static constexpr address firstCallsignsSkip = 0x71FB8B;

		__asm
		{
			mov ecx, dword ptr [esp + 0x28] // from "CallsignsCheck"

			cmp ecx, PATROL
			je conclusion // is "patrol"

			call GetCallsignsOffset // ecx: callsigns

			jmp dword ptr [firstCallsignsSkip]

			conclusion:
			jmp dword ptr [firstCallsignsExit]
		}
	}



	constexpr address secondCallsignsEntrance = 0x71FCCD;
	constexpr address secondCallsignsExit     = 0x71FCDD;

	// The second callsigns-specific part of the assignment function
	__declspec(naked) void SecondCallsigns()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x28] // from "CallsignsCheck"
			call GetCallsignsOffset         // ecx: callsigns

			jmp dword ptr [secondCallsignsExit]
		}
	}



	constexpr address collisionCalloutEntrance = 0x71C073;
	constexpr address collisionCalloutExit     = 0x71C08D;

	// Picks radio callouts in response to collisions with racers
	__declspec(naked) void CollisionCallout()
	{
		__asm
		{
			push eax                   // copType
			mov ecx, offset copTypeToBattalion
			call ModContainers::DefaultVaultMap<Battalion>::GetValue
			cmp eax, RHINO
			sete byte ptr [esp + 0x2B] // is "rhino"

			jmp dword ptr [collisionCalloutExit]
		}
	}



	constexpr address jurisdictionReportEntrance = 0x71D44D;
	constexpr address jurisdictionReportExit     = 0x71D48F;

	// Decides which jurisdiction to announce after a Heat-level change
	__declspec(naked) void JurisdictionReport()
	{
		static constexpr address jurisdictionReportSkip = 0x71D49A;

		__asm
		{
			mov eax, dword ptr [heatJurisdictionIDs.current]
			cmp eax, dword ptr [lastJurisdictionID]
			je skip // same jurisdiction

			mov dword ptr [lastJurisdictionID], eax

			cmp eax, CITY
			je skip // default jurisdiction

			push eax

			jmp dword ptr [jurisdictionReportExit]

			skip:
			jmp dword ptr [jurisdictionReportSkip]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ParseJurisdictions(const HeatParameters::Parser& parser)
	{
		Jurisdiction defaultJurisdiction = Jurisdiction::CITY;

		const auto NameToJurisdiction = [&defaultJurisdiction](const std::string_view name) -> Jurisdiction
		{
			if (name == "city")    return Jurisdiction::CITY;
			if (name == "state")   return Jurisdiction::STATE;
			if (name == "federal") return Jurisdiction::FEDERAL;

			return defaultJurisdiction;
		};

		constexpr std::string_view section = "Heat:Jurisdiction";

		// Parse string representations of jurisdictions first
		HeatParameters::Pair<std::string_view> jurisdictionNames("city");
		HeatParameters::Parse(parser, section, jurisdictionNames);

		// Validate and convert new "default" value (if applicable)
		std::string_view newDefaultName;

		if (parser.ParseFromFile<std::string_view>(section, HeatParameters::configDefaultKey, {newDefaultName}))
			defaultJurisdiction = NameToJurisdiction(newDefaultName);

		// Validate and convert Heat-level values
		for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
		{
			heatJurisdictionIDs.roam[heatLevelID] = NameToJurisdiction(jurisdictionNames.roam[heatLevelID]);
			heatJurisdictionIDs.race[heatLevelID] = NameToJurisdiction(jurisdictionNames.race[heatLevelID]);
		}
	}



	bool ParseCallsigns(const HeatParameters::Parser& parser)
	{
		std::vector<std::string_view> copVehicles;
		std::vector<std::string_view> battalionNames;

		parser.ParseUser<std::string_view, std::string_view>("Vehicles:Callsigns", copVehicles, {battalionNames});

		// Populate callsign map
		constexpr auto NameToBattalion = [](const std::string_view name) -> Battalion
		{
			if (name == "patrol") return Battalion::PATROL;
			if (name == "elite")  return Battalion::ELITE;
			if (name == "rhino")  return Battalion::RHINO;
			if (name == "cross")  return Battalion::CROSS;

			return Battalion::UNKNOWN;
		};

		constexpr auto IsBattalionValid = [](const Battalion value) -> bool {return (value != Battalion::UNKNOWN);};

		return copTypeToBattalion.FillFromVectors
		(
			"Vehicle-to-callsign",
			HeatParameters::configDefaultVaultHash,
			ModContainers::MapFillSetup(copVehicles,    Globals::GetVaultHash, Globals::IsVehicleTypeCar),
			ModContainers::MapFillSetup(battalionNames, NameToBattalion,       IsBattalionValid)
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// All these fix radio announcements for Heat levels > 5
		MemoryTools::MakeRangeNOP<0x71D345, 0x71D370>(); // Heat-level 1 filter

		MemoryTools::MakeRangeJMP<heatCheckEntrance,     heatCheckExit>    (HeatCheck);
		MemoryTools::MakeRangeJMP<heatReportEntrance,    heatReportExit>   (HeatReport);
		MemoryTools::MakeRangeJMP<playerPursuitEntrance, playerPursuitExit>(PlayerPursuit);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RAD] CopRadio");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Jurisdictions
		ParseJurisdictions(parser);

		// Callsigns
		if (ParseCallsigns(parser))
		{
			// Code modifications (conditional)
			MemoryTools::Write<byte>(0x24, {0x71FC00, 0x71FC04}); // free up stack variable

			MemoryTools::MakeRangeJMP<callsignsCheckEntrance,   callsignsCheckExit>  (CallsignsCheck);
			MemoryTools::MakeRangeJMP<firstCallsignsEntrance,   firstCallsignsExit>  (FirstCallsigns);
			MemoryTools::MakeRangeJMP<secondCallsignsEntrance,  secondCallsignsExit> (SecondCallsigns);
			MemoryTools::MakeRangeJMP<collisionCalloutEntrance, collisionCalloutExit>(CollisionCallout);
		}

		// Code modifications (general)
		MemoryTools::MakeRangeJMP<jurisdictionReportEntrance, jurisdictionReportExit>(JurisdictionReport);

		ApplyFixes(); // fixes announcements for Heat levels > 5

		// Status flag
		featureEnabled = true;

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		heatJurisdictionIDs.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger    .Log("    HEAT [RAD] CopRadio");
			heatJurisdictionIDs.Log("heatJurisdictionID      ");
		}
	}
}