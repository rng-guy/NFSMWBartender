#pragma once

#include <vector>
#include <string>
#include <fstream>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"



namespace CopDetection
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Data structures
	struct Settings
	{
		float radarRange;
		float patrolIconRange;
		float pursuitIconRange;
		bool  keepsIcon;
	};

	// Code caves
	bool isNewMiniMap = true;
	
	HashContainers::CachedVaultMap<Settings> copTypeToSettings({300.f, 0.f, 300.f, true}); // metres (x3), flag





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool __fastcall GetsMiniMapIcon(const address copVehicle) 
	{
		const address copAIVehicle = *reinterpret_cast<address*>(copVehicle + 0x54);

		bool& iconIsKept = *reinterpret_cast<bool*>(copAIVehicle - 0x4C + 0x769);
		if (iconIsKept) return true; // mini-map icon already kept

		bool& hasBeenInPursuit = *reinterpret_cast<bool*>(copAIVehicle - 0x4C + 0x76A);

		if (not hasBeenInPursuit)
		{
			const bool hasPursuit  = *reinterpret_cast<address*>(copAIVehicle + 0x70);
			const bool isRoadblock = *reinterpret_cast<address*>(copAIVehicle + 0x74);

			hasBeenInPursuit = (hasPursuit or isRoadblock);
		}

		if (not Globals::playerPerpVehicle) return false; // should never happen

		const Settings* const settings = copTypeToSettings.GetValue(Globals::GetVehicleType(copVehicle));
		if (not settings) return false; // should never happen

		const float iconRange = (hasBeenInPursuit) ? settings->pursuitIconRange : settings->patrolIconRange;

		if (iconRange > 0.f)
		{
			static const auto GetVehiclePosition = reinterpret_cast<address (__thiscall*)(address)>         (0x688340);
			static const auto GetSquaredDistance = reinterpret_cast<float   (__cdecl*)   (address, address)>(0x401930);

			const address copPosition = GetVehiclePosition(copVehicle);
			if (not copPosition) return false; // should never happen

			const address playerVehicle  = *reinterpret_cast<address*>(Globals::playerPerpVehicle - 0x758 + 0x4C - 0x4);
			const address playerPosition = GetVehiclePosition(playerVehicle);
			if (not playerPosition) return false; // should never happen

			if (GetSquaredDistance(copPosition, playerPosition) <= iconRange * iconRange)
			{
				iconIsKept = settings->keepsIcon;

				return true;
			}
		}
		
		return false;
	}



	float __fastcall GetRadarRange(const address copVehicle)
	{
		const Settings* const settings = copTypeToSettings.GetValue(Globals::GetVehicleType(copVehicle));
		return (settings) ? settings->radarRange : 0.f; // should never be false
	}



	std::fstream& operator<<
	(
		std::fstream&   stream,
		const Settings& copSettings
	) {
		const char* const delimiter = ", ";

		stream << copSettings.radarRange;
		stream << delimiter << copSettings.patrolIconRange;
		stream << delimiter << copSettings.pursuitIconRange;
		stream << delimiter << ((copSettings.keepsIcon) ? "true" : "false");

		return stream;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address colourUpdateEntrance = 0x579E16;
	constexpr address colourUpdateExit     = 0x579E1C;

	__declspec(naked) void ColourUpdate()
	{
		static constexpr float targetFrameTime     = 1.f / 30.f;
		static float           nextColourTimeStamp = 0.f;

		__asm
		{
			mov edx, dword ptr Globals::simulationTime
			mov ecx, eax

			fld dword ptr [edx]

			cmp byte ptr isNewMiniMap, 0x1
			je timestamp // new mini-map

			fcom dword ptr nextColourTimeStamp
			fnstsw ax
			test ah, 0x5
			jne conclusion // not yet time

			inc ecx

			timestamp:
			fadd dword ptr targetFrameTime
			fst dword ptr nextColourTimeStamp

			mov byte ptr isNewMiniMap, 0x0

			conclusion:
			fstp st(0)
			cmp ecx, 0x9

			jmp dword ptr colourUpdateExit
		}
	}



	constexpr address mapConstructorEntrance = 0x59DA9B;
	constexpr address mapConstructorExit     = 0x59DAA0;

	__declspec(naked) void MapConstructor()
	{
		__asm
		{
			mov byte ptr isNewMiniMap, 0x1

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x1C]
			pop edi

			jmp dword ptr mapConstructorExit
		}
	}



	constexpr address copVehicleIconEntrance = 0x579EDF;
	constexpr address copVehicleIconExit     = 0x579EE5;

	__declspec(naked) void CopVehicleIcon()
	{
		static constexpr address copVehicleIconSkip = 0x57A09F;

		__asm
		{
			// Execute original code first
			cmp eax, 0xB80933AA // "CHOPPER" class
			je conclusion       // is helicopter

			cmp byte ptr featureEnabled, 0x1
			jne limitation // map feature disabled

			mov ecx, dword ptr [esi]
			call GetsMiniMapIcon // ecx: copVehicle
			test al, al
			je skip              // no icon

			limitation:
			cmp dword ptr [esp + 0x18], 0x8
			jge skip // at icon cap

			conclusion:
			jmp dword ptr copVehicleIconExit

			skip:
			jmp dword ptr copVehicleIconSkip
		}
	}



	constexpr address copVehicleRadarEntrance = 0x6EE206;
	constexpr address copVehicleRadarExit     = 0x6EE20C;

	__declspec(naked) void CopVehicleRadar()
	{
		__asm
		{
			mov ecx, esi
			call GetRadarRange // ecx: copVehicle
			fxch st(1)
			fcompp

			jmp dword ptr copVehicleRadarExit
		}
	}



	constexpr address destructionCheckEntrance = 0x57A038;
	constexpr address destructionCheckExit     = 0x57A03D;

	__declspec(naked) void DestructionCheck()
	{
		__asm
		{
			// Execute original code first
			call dword ptr [edx + 0x14]
			test al, al
			je conclusion // not in pursuit

			mov ecx, dword ptr [esi]
			call Globals::IsVehicleDestroyed
			cmp al, 0x1

			conclusion:
			jmp dword ptr destructionCheckExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Also fixes the disappearing helicopter icon
		MemoryTools::Write<size_t>(0, {0x59D5CA});     // frame-counter initialisation
		MemoryTools::MakeRangeNOP(0x579EA2, 0x579EAB); // early icon-counter check
		MemoryTools::MakeRangeJMP(CopVehicleIcon,   copVehicleIconEntrance,   copVehicleIconExit);
		MemoryTools::MakeRangeJMP(DestructionCheck, destructionCheckEntrance, destructionCheckExit);

		// Fixes update frequency for cop-icon colours
		MemoryTools::MakeRangeJMP(ColourUpdate,   colourUpdateEntrance,   colourUpdateExit);
		MemoryTools::MakeRangeJMP(MapConstructor, mapConstructorEntrance, mapConstructorExit);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		std::vector<std::string> copVehicles;

		std::vector<float> radarRanges;
		std::vector<float> patrolIconRanges;
		std::vector<float> pursuitIconRanges;
		std::vector<bool>  keepsIcons;

		const size_t numCopVehicles = parser.ParseParameterTable<float, float, float, bool>
		(
			"Vehicles:Detection", 
			copVehicles, 
			{radarRanges,       0.f},
			{patrolIconRanges,  0.f},
			{pursuitIconRanges, 0.f},
			{keepsIcons}
		);

		if (numCopVehicles == 0) return false;

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
		{
			copTypeToSettings.try_emplace
			(
				Globals::GetVaultKey(copVehicles[vehicleID].c_str()), 
				radarRanges      [vehicleID], 
				patrolIconRanges [vehicleID], 
				pursuitIconRanges[vehicleID], 
				keepsIcons       [vehicleID]
			);
		}

		// Code modifications
		MemoryTools::MakeRangeNOP(0x579E33, 0x579E69); // pursuit check
		MemoryTools::MakeRangeNOP(0x579EE5, 0x579EEA); // non-pursuit icon flag
		MemoryTools::MakeRangeNOP(0x579EF0, 0x579F0E); // engagement-radius check
		MemoryTools::MakeRangeNOP(0x579FCD, 0x579FFD); // icon-flag checks

		MemoryTools::MakeRangeJMP(CopVehicleRadar, copVehicleRadarEntrance, copVehicleRadarExit);

		ApplyFixes(); // also contains map-icon feature

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [DET] CopDetection");

		copTypeToSettings.Validate
		(
			"Vehicle-to-settings",
			[=](const vault     key)   {return Globals::VehicleClassMatches(key, Globals::Class::CAR);},
			[=](const Settings& value) {return true;}
		);
	}
}