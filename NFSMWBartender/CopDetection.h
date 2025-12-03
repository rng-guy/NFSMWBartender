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

	// Code caves
	struct Settings
	{
		float radarRange;
		float patrolIconRange;
		float pursuitIconRange;
		bool  lockIcon;
	};

	HashContainers::CachedMap<vault, Settings> copTypeToSettings({300.f, 0.f, 50.f, true});





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool __fastcall IsInMapRange(const address copVehicle)
	{
		const address copAIVehicle = *reinterpret_cast<address*>(copVehicle + 0x54);
		if (not copAIVehicle) return false; // should never happen

		bool& iconIsLocked = *reinterpret_cast<bool*>(copAIVehicle - 0x4C + 0x769);
		if (iconIsLocked) return true; // mini-map icon locked in

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
			static const auto GetVehiclePosition = reinterpret_cast<address (__thiscall*)(address)>(0x688340);

			const address copPosition = GetVehiclePosition(copVehicle);
			if (not copPosition) return false; // should never happen

			const address playerVehicle  = *reinterpret_cast<address*>(Globals::playerPerpVehicle - 0x758 + 0x4C - 0x4);
			const address playerPosition = GetVehiclePosition(playerVehicle);
			if (not playerPosition) return false; // should never happen

			static const auto GetSquaredDistance = reinterpret_cast<float (__cdecl*)(address, address)>(0x401930);
			const float       copDistanceSquared = GetSquaredDistance(copPosition, playerPosition);

			if (copDistanceSquared <= iconRange * iconRange)
			{
				iconIsLocked = settings->lockIcon;

				return true;
			}
		}
		
		return false;
	}



	float __fastcall GetRadarRange(const address copVehicle)
	{
		const Settings* const settings = copTypeToSettings.GetValue(Globals::GetVehicleType(copVehicle));
		return (settings) ? settings->radarRange : 0.f;
	}



	std::fstream& operator<<
	(
		std::fstream&   stream,
		const Settings& copSettings
	) {
		stream << copSettings.radarRange;
		stream << ", " << copSettings.patrolIconRange;
		stream << ", " << copSettings.pursuitIconRange;
		stream << ", " << ((copSettings.lockIcon) ? "true" : "false");

		return stream;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copVehicleIconEntrance = 0x579EDF;
	constexpr address copVehicleIconExit     = 0x579EE5;

	__declspec(naked) void CopVehicleIcon()
	{
		static constexpr address copVehicleIconSkip = 0x57A09F;

		__asm
		{
			// Execute original code first
			cmp eax, 0xB80933AA
			je conclusion // is helicopter

			cmp dword ptr [esp + 0x18], 0x8
			jge skip // at vehicle icon cap

			cmp byte ptr featureEnabled, 0x1
			jne conclusion // map feature disabled

			mov ecx, dword ptr [esi]
			call IsInMapRange // ecx: copVehicle
			test al, al
			je skip           // not in range

			conclusion:
			jmp dword ptr copVehicleIconExit

			skip:
			jmp dword ptr copVehicleIconSkip
		}
	}



	constexpr address resetInternalsEntrance = 0x416B7A;
	constexpr address resetInternalsExit     = 0x416B80;

	__declspec(naked) void ResetInternals()
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x768], al

			mov byte ptr [esi + 0x769], al
			mov byte ptr [esi + 0x76A], al

			jmp dword ptr resetInternalsExit
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





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		static bool fixesApplied = false;

		if (fixesApplied) return;
		
		// Also fixes the disappearing helicopter icon
		MemoryTools::MakeRangeNOP(0x579EA2, 0x579EAB); // early icon-counter check
		MemoryTools::MakeRangeJMP(CopVehicleIcon, copVehicleIconEntrance, copVehicleIconExit);
		
		fixesApplied = true;
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathBasic + "Cosmetic.ini")) return false;

		std::vector<std::string> copVehicles;

		std::vector<float> radarRanges;
		std::vector<float> patrolIconRanges;
		std::vector<float> pursuitIconRanges;
		std::vector<bool>  lockIcons;

		const size_t numCopVehicles = parser.ParseParameterTable<float, float, float, bool>
		(
			"Vehicles:Detection", 
			copVehicles, 
			{radarRanges,       0.f},
			{patrolIconRanges,  0.f},
			{pursuitIconRanges, 0.f},
			{lockIcons}
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
				lockIcons        [vehicleID]
			);
		}

		// Code modifications
		MemoryTools::MakeRangeNOP(0x579EF0, 0x579F0E); // engagement-radius check

		MemoryTools::MakeRangeJMP(CopVehicleRadar, copVehicleRadarEntrance, copVehicleRadarExit);
		MemoryTools::MakeRangeJMP(ResetInternals,  resetInternalsEntrance,  resetInternalsExit);

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