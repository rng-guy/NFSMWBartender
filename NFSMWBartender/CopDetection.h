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
	bool isNewMiniMap  = true;
	bool isNewWorldMap = true;

	bool updateWorldMapColours = false;

	HashContainers::CachedVaultMap<Settings> copTypeToSettings({300.f, 0.f, 300.f, true}); // metres (x3), flag





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	bool __stdcall ShouldUpdateColours
	(
		float&     updateTimestamp,
		bool&      isFirstQuery,
		const bool unpaused
	) {
		const float gameTime = Globals::GetGameTime(unpaused);

		if (isFirstQuery or (gameTime >= updateTimestamp))
		{
			constexpr float targetFrameTime = 1.f / 30.f; // seconds

			updateTimestamp = gameTime + targetFrameTime;
			isFirstQuery    = false;

			return true;
		}

		return false;
	}



	bool __fastcall GetsMiniMapIcon(const address copVehicle) 
	{
		const address copAIVehicle = *reinterpret_cast<address*>(copVehicle + 0x54);

		bool& iconIsKept = *reinterpret_cast<bool*>(copAIVehicle - 0x4C + 0x81); // padding byte
		if (iconIsKept) return true; // mini-map icon already kept

		bool& hasBeenInPursuit = *reinterpret_cast<bool*>(copAIVehicle - 0x4C + 0x82); // padding byte

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
			const auto GetVehiclePosition = reinterpret_cast<address (__thiscall*)(address)>         (0x688340);
			const auto GetSquaredDistance = reinterpret_cast<float   (__cdecl*)   (address, address)>(0x401930);

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

	constexpr address worldMapUpdateEntrance = 0x55B770;
	constexpr address worldMapUpdateExit     = 0x55B776;

	// Makes cop icons on the world map flash at a consistent pace
	__declspec(naked) void WorldMapUpdate()
	{
		static float updateTimestamp = 0.f; // seconds

		__asm
		{
			push 0x0                    // unpaused
			push offset isNewWorldMap   // isFirstQuery
			push offset updateTimestamp // updateTimestamp
			call ShouldUpdateColours
			mov byte ptr updateWorldMapColours, al

			// Execute original code and resume
			mov edi, dword ptr [esi + 0x154]

			jmp dword ptr worldMapUpdateExit
		}
	}



	constexpr address copVehicleIconEntrance = 0x579EDF;
	constexpr address copVehicleIconExit     = 0x579EE5;

	// Decides which cop vehicle gets a mini-map icon
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
			cmp dword ptr [esp + 0x18], 0x8 // icon count
			jge skip                        // at icon cap

			conclusion:
			jmp dword ptr copVehicleIconExit

			skip:
			jmp dword ptr copVehicleIconSkip
		}
	}



	constexpr address copVehicleRadarEntrance = 0x6EE206;
	constexpr address copVehicleRadarExit     = 0x6EE20C;

	// Sets the cop radar's detection range for cop vehicles
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



	constexpr address destructionCheckEntrance = 0x57A034;
	constexpr address destructionCheckExit     = 0x57A03D;

	// Ensures destroyed roadblock vehicles get white mini-map icons
	__declspec(naked) void DestructionCheck()
	{
		__asm
		{
			// Execute original code first
			cmp byte ptr [edi + 0x8], 0x0
			je conclusion // not in pursuit

			mov ecx, dword ptr [esi]
			call Globals::IsVehicleDestroyed
			cmp al, 0x1

			conclusion:
			jmp dword ptr destructionCheckExit
		}
	}



	constexpr address miniMapCopColoursEntrance = 0x579E16;
	constexpr address miniMapCopColoursExit     = 0x579E1C;

	// Makes cop icons on the mini-map flash at a consistent pace
	__declspec(naked) void MiniMapCopColours()
	{
		static float updateTimestamp = 0.f; // seconds

		__asm
		{
			push eax

			push 0x1                    // unpaused
			push offset isNewMiniMap    // isFirstQuery
			push offset updateTimestamp // updateTimestamp
			call ShouldUpdateColours
			test al, al

			pop ecx
			je conclusion // not yet time

			inc ecx

			conclusion:
			cmp ecx, 0x9

			jmp dword ptr miniMapCopColoursExit
		}
	}



	constexpr address worldMapCopColoursEntrance = 0x51F70C;
	constexpr address worldMapcopColoursExit     = 0x51F712;

	// Makes cop icons on the world map flash at a consistent pace
	__declspec(naked) void WorldMapCopColours()
	{
		__asm
		{
			mov ecx, dword ptr [esi + 0x38]

			cmp byte ptr updateWorldMapColours, 0x1
			jne conclusion // not yet time

			inc ecx

			conclusion:
			mov eax, ecx

			jmp dword ptr worldMapcopColoursExit
		}
	}



	constexpr address miniMapConstructorEntrance = 0x59DA9B;
	constexpr address miniMapConstructorExit     = 0x59DAA0;

	// Prepares the cop-icon state when a new mini-map is created
	__declspec(naked) void MiniMapConstructor()
	{
		__asm
		{
			mov byte ptr isNewMiniMap, 0x1

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x1C]
			pop edi

			jmp dword ptr miniMapConstructorExit
		}
	}



	constexpr address worldMapConstructorEntrance = 0x5614FF;
	constexpr address worldMapConstructorExit     = 0x561505;

	// Prepares the cop-icon state when a new world map is created
	__declspec(naked) void WorldMapConstructor()
	{
		__asm
		{
			mov byte ptr isNewWorldMap, 0x1

			// Execute original code and resume
			mov dword ptr [esi + 0x124], eax

			jmp dword ptr worldMapConstructorExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Also fixes the disappearing helicopter icon
		MemoryTools::MakeRangeNOP(0x579EA2, 0x579EAB); // early icon-counter check
		MemoryTools::MakeRangeJMP(CopVehicleIcon,   copVehicleIconEntrance,   copVehicleIconExit);
		MemoryTools::MakeRangeJMP(DestructionCheck, destructionCheckEntrance, destructionCheckExit);

		// Fixes update frequency for cop-icon colours
		MemoryTools::MakeRangeJMP(WorldMapUpdate,      worldMapUpdateEntrance,      worldMapUpdateExit);
		MemoryTools::MakeRangeJMP(MiniMapCopColours,   miniMapCopColoursEntrance,   miniMapCopColoursExit);
		MemoryTools::MakeRangeJMP(WorldMapCopColours,  worldMapCopColoursEntrance,  worldMapcopColoursExit);
		MemoryTools::MakeRangeJMP(MiniMapConstructor,  miniMapConstructorEntrance,  miniMapConstructorExit);
		MemoryTools::MakeRangeJMP(WorldMapConstructor, worldMapConstructorEntrance, worldMapConstructorExit);
	}



	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [DET] CopDetection");

		if (not parser.LoadFileWithLog(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Detection settings
		std::vector<std::string> copVehicles;

		std::vector<float> radarRanges;
		std::vector<float> patrolIconRanges;
		std::vector<float> pursuitIconRanges;
		std::vector<bool>  keepsIcons;

		const size_t numCopVehicles = parser.ParseTable<float, float, float, bool>
		(
			"Vehicles:Detection", 
			copVehicles, 
			{radarRanges,       {0.f}},
			{patrolIconRanges,  {0.f}},
			{pursuitIconRanges, {0.f}},
			{keepsIcons}
		);

		std::vector<Settings> settings(numCopVehicles);
	
		for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
		{
			settings[vehicleID] =
			{
				radarRanges      [vehicleID],
				patrolIconRanges [vehicleID],
				pursuitIconRanges[vehicleID],
				keepsIcons       [vehicleID]
			};
		}

		const bool mapIsValid = copTypeToSettings.FillFromVectors<std::string, Settings>
		(
			"Vehicle-to-settings",
			HeatParameters::defaultValueHandle,
			copVehicles,
			Globals::StringToVaultKey,
			Globals::IsVehicleCar,
			settings,
			[=](const Settings& settings) -> Settings {return settings;},
			[=](const Settings& settings) -> bool     {return true;}
		);

		if (not mapIsValid) return false;

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
}