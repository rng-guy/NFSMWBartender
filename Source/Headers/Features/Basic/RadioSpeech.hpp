#pragma once

#include <vector>
#include <string_view>

#include "../../Common/Globals.hpp"

#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace RadioSpeech
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[RAD]";
	constexpr Globals::LogLiteral logName = "RadioSpeech";

	// Enums
	enum class Jurisdiction : int
	{
		CITY    = 0,
		STATE   = 1,
		FEDERAL = 2
	};
	
	enum class Battalion
	{
		UNKNOWN,
		PATROL,
		ELITE,
		RHINO,
		CROSS
	};

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(Jurisdiction, heatJurisdictionID, Jurisdiction::CITY);

	// Vehicle maps
	RELEASE_CONSTINIT VEHICLE_MAP(Battalion, copTypeToBattalion, Battalion::PATROL);

	// ASM detours
	size_t lastReportedHeatLevel = 1;
	int    lastJurisdictionID    = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] ptrdiff_t __fastcall GetCallsignsOffset(const Battalion battalion)
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





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Checks whether a new Heat-level announcement is due
	ASSEMBLY_DETOUR(HeatCheck, /* begin = */ 0x71D370, /* end = */ 0x71D3AE)
	{
		static constexpr address silenceExit = 0x71D433;

		__asm
		{
			mov ebp, dword ptr [esi + 0x104] // current Heat level

			cmp ebp, dword ptr [lastReportedHeatLevel]
			je silence // already reported

			mov dword ptr [lastReportedHeatLevel], ebp

			cmp ebp, 10
			jg silence // new Heat Level > 10

			sub ebp, 2
			jl silence // new Heat level < 2

			EXIT_ASSEMBLY_DETOUR(HeatCheck)

			silence:
			jmp dword ptr [silenceExit]
		}
	}



	// Converts the Heat-level ID into a copspeech offset
	ASSEMBLY_DETOUR(HeatReport, 0x71D428, 0x71D42F)
	{
		__asm
		{
			xchg ecx, ebp // ebp from "HeatCheck"

			mov eax, 1
			shl eax, cl

			mov ecx, ebp

			EXIT_ASSEMBLY_DETOUR(HeatReport)
		}
	}



	// Resets transition state whenever a new player pursuit begins
	ASSEMBLY_DETOUR(PlayerPursuit, 0x704F70, 0x704F76)
	{
		using enum Jurisdiction;

		__asm
		{
			mov dword ptr [lastReportedHeatLevel], 1
			mov dword ptr [lastJurisdictionID], CITY

			// Execute original code and resume
			mov dword ptr [esi + 0x130], edi

			EXIT_ASSEMBLY_DETOUR(PlayerPursuit)
		}
	}



	// Retrieves the current vehicle's callsign
	ASSEMBLY_DETOUR(CallsignsCheck, 0x71FB01, 0x71FB06)
	{
		using enum Battalion;

		__asm
		{
			push eax // copType
			mov ecx, offset copTypeToBattalion
			call ModContainers::VehicleMap<Battalion>::GetValue
			cmp eax, CROSS

			mov dword ptr [esp + 0x28], eax // freed variable

			EXIT_ASSEMBLY_DETOUR(CallsignsCheck)
		}
	}



	// The first callsigns-specific part of the assignment function
	ASSEMBLY_DETOUR(FirstCallsigns, 0x71FB27, 0x71FB76)
	{
		static constexpr address specialExit = 0x71FB8B;

		using enum Battalion;

		__asm
		{
			mov ecx, dword ptr [esp + 0x28] // from "CallsignsCheck"

			cmp ecx, PATROL
			jne special // not "patrol"

			EXIT_ASSEMBLY_DETOUR(FirstCallsigns)

			special:
			call GetCallsignsOffset // ecx: battalion

			jmp dword ptr [specialExit]
		}
	}



	// The second callsigns-specific part of the assignment function
	ASSEMBLY_DETOUR(SecondCallsigns, 0x71FCCD, 0x71FCDD)
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x28] // from "CallsignsCheck"
			call GetCallsignsOffset         // ecx: battalion

			EXIT_ASSEMBLY_DETOUR(SecondCallsigns)
		}
	}



	// Picks radio callouts in response to collisions with racers
	ASSEMBLY_DETOUR(CollisionCallout, 0x71C073, 0x71C08D)
	{
		using enum Battalion;

		__asm
		{
			push eax                   // copType
			mov ecx, offset copTypeToBattalion
			call ModContainers::VehicleMap<Battalion>::GetValue
			cmp eax, RHINO
			sete byte ptr [esp + 0x2B] // is "rhino"

			EXIT_ASSEMBLY_DETOUR(CollisionCallout)
		}
	}



	// Decides which jurisdiction to announce after a Heat-level change
	ASSEMBLY_DETOUR(JurisdictionReport, 0x71D44D, 0x71D48F)
	{
		static constexpr address silenceExit = 0x71D49A;

		using enum Jurisdiction;

		__asm
		{
			mov eax, dword ptr [heatJurisdictionID.current]
			cmp eax, dword ptr [lastJurisdictionID]
			je silence // same jurisdiction

			mov dword ptr [lastJurisdictionID], eax

			cmp eax, CITY
			je silence // default jurisdiction

			push eax

			EXIT_ASSEMBLY_DETOUR(JurisdictionReport)

			silence:
			jmp dword ptr [silenceExit]
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	void ExtractJurisdictions(const ConfigParser::Parser& parser)
	{
		constexpr auto NameToJurisdiction = [](const std::string_view name) -> Jurisdiction
		{
			if (name == "city")    return Jurisdiction::CITY;
			if (name == "state")   return Jurisdiction::STATE;
			if (name == "federal") return Jurisdiction::FEDERAL;

			return heatJurisdictionID.current;
		};

		const auto* const section = parser.GetSection("Heat:Jurisdiction");

		// Extract string representations of jurisdictions first
		HEAT_PARAMETER_VALUE(std::string_view, jurisdictionNames, "city");

		HeatParameters::Extract(section, jurisdictionNames); // initialises values regardless of extraction success

		// Extract new "default" value
		std::string_view newDefaultName;

		if (ConfigParser::Parser::ExtractScalars<std::string_view>(section, Globals::defaultKey, {newDefaultName}))
			heatJurisdictionID.current = NameToJurisdiction(newDefaultName); // string "default" may be invalid enum

		// Validate and convert Heat-level values
		for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
		{
			heatJurisdictionID.roam[heatLevelID] = NameToJurisdiction(jurisdictionNames.roam[heatLevelID]);
			heatJurisdictionID.race[heatLevelID] = NameToJurisdiction(jurisdictionNames.race[heatLevelID]);
		}
	}



	[[nodiscard]] bool ExtractBattalions(const ConfigParser::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<std::string_view> battalionNames;

		parser.ExtractVectors("Vehicles:Callsigns", copNames, ConfigParser::VectorField(battalionNames));

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

		return copTypeToBattalion.Fill
		(
			ModContainers::FillSetup(copNames,       Globals::GetVaultHash, Globals::IsVehicleTypeCar),
			ModContainers::FillSetup(battalionNames, NameToBattalion,       IsBattalionValid)
		);
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Radio announcements for Heat levels > 5
		MemoryTools::MakeRangeNOP<0x71D345, 0x71D370>(); // Heat-level filter

		PATCH_ASSEMBLY_DETOUR(HeatCheck);
		PATCH_ASSEMBLY_DETOUR(HeatReport);
		PATCH_ASSEMBLY_DETOUR(PlayerPursuit);
	}



	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileCosmetic)) return false;

		// Jurisdictions
		ExtractJurisdictions(parser);

		// Callsign battalions
		if (ExtractBattalions(parser))
		{
			// Code modifications (conditional)
			MemoryTools::Write<byte>(0x24, {0x71FC00, 0x71FC04}); // free up stack variable

			PATCH_ASSEMBLY_DETOUR(CallsignsCheck);
			PATCH_ASSEMBLY_DETOUR(FirstCallsigns);
			PATCH_ASSEMBLY_DETOUR(SecondCallsigns);
			PATCH_ASSEMBLY_DETOUR(CollisionCallout);
		}

		// Code modifications (general)
		PATCH_ASSEMBLY_DETOUR(JurisdictionReport);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		heatJurisdictionID.SetToHeatState(state);
	}
}