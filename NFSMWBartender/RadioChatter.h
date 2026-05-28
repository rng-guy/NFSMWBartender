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

	// Data structures
	enum Jurisdiction // as above
	{
		CITY    = 0,
		STATE   = 1,
		FEDERAL = 2
	};
	
	enum Callsigns // C-style for ASM
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

	constinit ModContainers::DefaultCopyVaultMap<Callsigns> copTypeToCallsignID(Callsigns::PATROL);





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

			cmp ebp, dword ptr lastReportedHeatLevel
			je skip // already reported

			mov dword ptr lastReportedHeatLevel, ebp

			cmp ebp, 0xA
			jg skip // new Heat Level > 10

			sub ebp, 0x2
			jl skip // new Heat level < 2

			jmp dword ptr heatCheckExit

			skip:
			jmp dword ptr heatCheckSkip
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

			mov eax, 0x1
			shl eax, cl

			mov ecx, ebp

			jmp dword ptr heatReportExit
		}
	}



	constexpr address playerPursuitEntrance = 0x704F70;
	constexpr address playerPursuitExit     = 0x704F76;

	// Resets transition state whenever a new player pursuit begins
	__declspec(naked) void PlayerPursuit()
	{
		__asm
		{
			mov dword ptr lastReportedHeatLevel, 0x1
			mov dword ptr lastJurisdictionID, CITY

			// Execute original code and resume
			mov dword ptr [esi + 0x130], edi

			jmp dword ptr playerPursuitExit
		}
	}



	constexpr address crossCallsignEntrance = 0x71FB01;
	constexpr address crossCallsignExit     = 0x71FB06;

	// Checks whether the current vehicle gets Cross' callsign
	__declspec(naked) void CrossCallsign()
	{
		__asm
		{
			push eax // copType
			mov ecx, offset copTypeToCallsignID
			call ModContainers::DefaultCopyVaultMap<Callsigns>::GetValue
			cmp eax, CROSS

			mov dword ptr [esp + 0x28], eax // repurposed variable

			jmp dword ptr crossCallsignExit
		}
	}



	constexpr address firstCallsignEntrance = 0x71FB27;
	constexpr address firstCallsignExit     = 0x71FB76;

	// The first callsign-specific part of the assignment function
	__declspec(naked) void FirstCallsign()
	{
		static constexpr address firstCallsignSkip = 0x71FB8B;

		__asm
		{
			mov edx, dword ptr [esp + 0x28] // from "CrossCallsign"

			cmp edx, PATROL
			je conclusion // is "patrol"

			mov eax, 0x10

			mov ecx, 0x20
			cmp edx, RHINO
			cmove eax, ecx // is "rhino"

			jmp dword ptr firstCallsignSkip

			conclusion:
			jmp dword ptr firstCallsignExit
		}
	}



	constexpr address secondCallsignEntrance = 0x71FCCD;
	constexpr address secondCallsignExit     = 0x71FCDD;

	// The second callsign-specific part of the assignment function
	__declspec(naked) void SecondCallsign()
	{
		__asm
		{
			mov eax, 0x10
			mov edx, dword ptr [esp + 0x28] // from "CrossCallsign"

			mov ecx, -0x1
			cmp edx, PATROL
			cmove eax, ecx // is "patrol"

			mov ecx, 0x20
			cmp edx, RHINO
			cmove eax, ecx // is "rhino"

			jmp dword ptr secondCallsignExit
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
			mov ecx, offset copTypeToCallsignID
			call ModContainers::DefaultCopyVaultMap<Callsigns>::GetValue
			cmp eax, RHINO
			sete byte ptr [esp + 0x2B] // is "rhino"

			jmp dword ptr collisionCalloutExit
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
			mov eax, dword ptr heatJurisdictionIDs.current
			cmp eax, dword ptr lastJurisdictionID
			je skip // same jurisdiction

			mov dword ptr lastJurisdictionID, eax

			cmp eax, CITY
			je skip // default jurisdiction

			push eax

			jmp dword ptr jurisdictionReportExit

			skip:
			jmp dword ptr jurisdictionReportSkip
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
		std::vector<const char*>      copVehicles; // C-style for game compatibility
		std::vector<std::string_view> callsignNames;

		parser.ParseUser<const char*, std::string_view>("Vehicles:Callsigns", copVehicles, {callsignNames});

		// Populate callsign map
		constexpr auto NameToCallsigns = [](const std::string_view name) -> Callsigns
		{
			if (name == "patrol") return Callsigns::PATROL;
			if (name == "elite")  return Callsigns::ELITE;
			if (name == "rhino")  return Callsigns::RHINO;
			if (name == "cross")  return Callsigns::CROSS;

			return Callsigns::UNKNOWN;
		};

		constexpr auto AreCallsignsValid = [](const Callsigns value) -> bool {return (value != Callsigns::UNKNOWN);};

		return copTypeToCallsignID.FillFromVectors
		(
			"Vehicle-to-callsign",
			Globals::GetVaultKey(HeatParameters::configDefaultKey),
			ModContainers::FillSetup(copVehicles,   Globals::GetVaultKey, Globals::IsVehicleTypeCar),
			ModContainers::FillSetup(callsignNames, NameToCallsigns,      AreCallsignsValid)
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
			// Code modifications (feature-specific)
			MemoryTools::Write<byte>(0x24, {0x71FC00, 0x71FC04}); // free up stack variable

			MemoryTools::MakeRangeJMP<crossCallsignEntrance,    crossCallsignExit>   (CrossCallsign);
			MemoryTools::MakeRangeJMP<firstCallsignEntrance,    firstCallsignExit>   (FirstCallsign);
			MemoryTools::MakeRangeJMP<secondCallsignEntrance,   secondCallsignExit>  (SecondCallsign);
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