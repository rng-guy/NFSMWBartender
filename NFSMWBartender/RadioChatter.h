#pragma once

#include <string>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
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

	// Data pointers
	float& lastReportedHeat = *reinterpret_cast<float*>(0x9020A0);

	// Heat parameters
	HeatParameters::Pair<int> heatJurisdictionIDs(Jurisdiction::CITY);

	// Code caves 
	HashContainers::CachedVaultMap<Callsigns> copTypeToCallsignID(Callsigns::PATROL);

	size_t lastReportedHeatLevel = 1;
	int    lastJurisdictionID    = heatJurisdictionIDs.current;





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatCheckEntrance = 0x71D370;
	constexpr address heatCheckExit     = 0x71D3AE;

	__declspec(naked) void HeatCheck()
	{
		static constexpr address heatCheckSkip = 0x71D44D;

		__asm
		{
			mov ebp, dword ptr [esi + 0x104]

			cmp ebp, dword ptr lastReportedHeatLevel
			je skip // already reported

			mov dword ptr lastReportedHeatLevel, ebp

			mov edx, dword ptr lastReportedHeat
			mov eax, dword ptr [ebx + 0x1C]
			mov dword ptr [edx], eax

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

	__declspec(naked) void HeatReport()
	{
		__asm
		{
			xchg ecx, ebp

			mov eax, 0x1
			shl eax, cl

			mov ecx, ebp

			jmp dword ptr heatReportExit
		}
	}



	constexpr address playerPursuitEntrance = 0x704F70;
	constexpr address playerPursuitExit     = 0x704F76;

	__declspec(naked) void PlayerPursuit()
	{
		__asm
		{
			mov eax, dword ptr lastReportedHeat
			mov dword ptr [eax], 0x3F800000 // 1.f

			mov dword ptr lastReportedHeatLevel, 0x1
			mov dword ptr lastJurisdictionID, CITY

			// Execute original code and resume
			mov dword ptr [esi + 0x130], edi

			jmp dword ptr playerPursuitExit
		}
	}



	constexpr address crossCallsignEntrance = 0x71FB01;
	constexpr address crossCallsignExit     = 0x71FB06;

	__declspec(naked) void CrossCallsign()
	{
		__asm
		{
			push eax // copType
			mov ecx, offset copTypeToCallsignID
			call HashContainers::CachedVaultMap<Callsigns>::GetValue

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

	__declspec(naked) void SecondCallsign()
	{
		__asm
		{
			mov eax, 0x10
			mov edx, dword ptr [esp + 0x28]

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

	__declspec(naked) void CollisionCallout()
	{
		__asm
		{
			push eax // copType
			mov ecx, offset copTypeToCallsignID
			call HashContainers::CachedVaultMap<Callsigns>::GetValue

			cmp eax, RHINO
			sete byte ptr [esp + 0x2B] // is "rhino"

			jmp dword ptr collisionCalloutExit
		}
	}



	constexpr address jurisdictionReportEntrance = 0x71D44D;
	constexpr address jurisdictionReportExit     = 0x71D48F;

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





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		MemoryTools::Write<byte>(0x1, {0x71D356}); // remove Heat-level 1 skip

		// Fixes radio announcements for Heat levels > 5
		MemoryTools::MakeRangeJMP(HeatCheck,     heatCheckEntrance,     heatCheckExit);
		MemoryTools::MakeRangeJMP(HeatReport,    heatReportEntrance,    heatReportExit);
		MemoryTools::MakeRangeJMP(PlayerPursuit, playerPursuitEntrance, playerPursuitExit);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		// Jurisdictions
		HeatParameters::Pair<std::string> jurisdictionNames("city");

		const HashContainers::Map<std::string, Jurisdiction> nameToJurisdiction =
		{
			{"city",    Jurisdiction::CITY},
			{"state",   Jurisdiction::STATE},
			{"federal", Jurisdiction::FEDERAL}
		};

		HeatParameters::Parse<std::string>(parser, "Heat:Jurisdiction", {jurisdictionNames});

		for (const bool forRaces : {false, true})
		{
			auto&       jurisdictionIDs = heatJurisdictionIDs.GetValues(forRaces);
			const auto& names           = jurisdictionNames  .GetValues(forRaces);

			for (const size_t heatLevel : HeatParameters::heatLevels)
			{
				const auto foundName = nameToJurisdiction.find(names[heatLevel - 1]);

				jurisdictionIDs[heatLevel - 1] = (foundName != nameToJurisdiction.end()) ? foundName->second : heatJurisdictionIDs.current;
			}
		}

		// Callsigns
		const HashContainers::Map<std::string, Callsigns> nameToCallsigns =
		{ 
			{"patrol", Callsigns::PATROL},
			{"elite",  Callsigns::ELITE},
			{"rhino",  Callsigns::RHINO},
			{"cross",  Callsigns::CROSS}
		};

		std::vector<std::string> copVehicles;
		std::vector<std::string> callsignNames;

		const size_t numCopVehicles = parser.ParseUserParameter<std::string>("Vehicles:Callsigns", copVehicles, callsignNames);

		if (numCopVehicles > 0)
		{
			for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
			{
				const auto      foundName = nameToCallsigns.find(callsignNames[vehicleID]);
				const Callsigns callsigns = (foundName != nameToCallsigns.end()) ? foundName->second : Callsigns::UNKNOWN;

				copTypeToCallsignID.try_emplace(Globals::GetVaultKey(copVehicles[vehicleID].c_str()), callsigns);
			}

			// Code modifications (feature-specific)
			MemoryTools::Write<byte>(0x24, {0x71FC00, 0x71FC04}); // free up superfluous stack variable

			MemoryTools::MakeRangeJMP(CrossCallsign,    crossCallsignEntrance,    crossCallsignExit);
			MemoryTools::MakeRangeJMP(FirstCallsign,    firstCallsignEntrance,    firstCallsignExit);
			MemoryTools::MakeRangeJMP(SecondCallsign,   secondCallsignEntrance,   secondCallsignExit);
			MemoryTools::MakeRangeJMP(CollisionCallout, collisionCalloutEntrance, collisionCalloutExit);
			
		}

		// Code modifications (general)
		MemoryTools::MakeRangeJMP(JurisdictionReport, jurisdictionReportEntrance, jurisdictionReportExit);

		ApplyFixes(); // fixes announcements for Heat levels > 5

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (copTypeToCallsignID.empty()) return;
	
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [RAD] RadioChatter");

		copTypeToCallsignID.Validate
		(
			"Vehicle-to-callsign",
			[=](const vault     key)   {return Globals::VehicleClassMatches(key, Globals::Class::CAR);},
			[=](const Callsigns value) {return (value != Callsigns::UNKNOWN);}
		);
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