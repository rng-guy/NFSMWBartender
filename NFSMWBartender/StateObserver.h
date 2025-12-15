#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "DestructionStrings.h"
#include "HelicopterVision.h"
#include "InteractiveMusic.h"
#include "RadioCallsigns.h"
#include "CopDetection.h"

#include "GeneralSettings.h"
#include "GroundSupports.h"

#include "PursuitObserver.h"
#include "CopSpawnOverrides.h"



namespace StateObserver
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves
	size_t playerHeatLevel = 0;
	bool   playerIsRacing  = false;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void OnHeatLevelUpdates()
	{
		// Shouldn't be necessary, but this is a BlackBox game..
		if ((playerHeatLevel >= 1) and (playerHeatLevel <= HeatParameters::maxHeatLevel))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("    HEAT [STA] Heat level now", static_cast<int>(playerHeatLevel), (playerIsRacing) ? "(race)" : "(free-roam)");

			Globals::playerHeatLevelKnown = true;

			GeneralSettings::SetToHeat(playerIsRacing, playerHeatLevel);
			GroundSupports ::SetToHeat(playerIsRacing, playerHeatLevel);
			PursuitObserver::SetToHeat(playerIsRacing, playerHeatLevel);
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [STA] Invalid Heat level", static_cast<int>(playerHeatLevel), (playerIsRacing) ? "(race)" : "(free-roam)");
	}



	void OnGameStartUpdates()
	{
		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Open("BartenderLog.txt");
			Globals::logger.Log ("\n SESSION [VER] Bartender v2.05.00");

			Globals::logger.LogLongIndent("Basic    feature set", (Globals::basicSetEnabled)    ? "enabled" : "disabled");
			Globals::logger.LogLongIndent("Advanced feature set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");

			if (MemoryTools::numRangeErrors > 0)
				Globals::logger.LogLongIndent("[MEM]", static_cast<int>(MemoryTools::numRangeErrors), "RANGE ERRORS!");

			if (MemoryTools::numCaveErrors > 0)
				Globals::logger.LogLongIndent("[MEM]", static_cast<int>(MemoryTools::numCaveErrors), "CAVE ERRORS!");
		}

		DestructionStrings::Validate();
		
		if constexpr (Globals::loggingEnabled)
		{
			HelicopterVision::LogSetupReport();
			InteractiveMusic::LogSetupReport();
		}

		RadioCallsigns::Validate();
		CopDetection  ::Validate();

		GroundSupports ::Validate();
		PursuitObserver::Validate();
	}



	void OnGameplayUpdates()
	{
		static const auto IsRacing = reinterpret_cast<bool (__thiscall*)(address)>(0x409500);

		if (Globals::playerPerpVehicle and (playerIsRacing != IsRacing(Globals::playerPerpVehicle)))
		{
			playerIsRacing = (not playerIsRacing);

			OnHeatLevelUpdates();
		}
		
		PursuitObserver::UpdateState();
	}



	void OnWorldLoadUpdates()
	{
		CopSpawnOverrides::ResetState();
	}



	void OnRetryUpdates()
	{
		playerHeatLevel = 0; // forces an update

		Globals::playerHeatLevelKnown = false;

		CopSpawnOverrides::ResetState();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatLevelObserverEntrance = 0x4090BE;
	constexpr address heatLevelObserverExit     = 0x4090C6;

	// Triggers when loading into areas or during pursuits
	__declspec(naked) void HeatLevelObserver()
	{
		__asm
		{
			cmp ebp, dword ptr playerHeatLevel
			je conclusion // Heat unchanged

			cmp esi, dword ptr Globals::playerPerpVehicle
			jne conclusion // not player vehicle

			cmp dword ptr [esp + 0x20], 0x416A75
			je conclusion // not true Heat

			mov dword ptr playerHeatLevel, ebp
			call OnHeatLevelUpdates

			conclusion:
			// Execute original code and resume
			cmp bl, byte ptr [esi + 0x2E]
			setne al
			cmp ebp, edi

			jmp dword ptr heatLevelObserverExit
		}
	}



	constexpr address gameStartObserverEntrance = 0x570620;
	constexpr address gameStartObserverExit     = 0x570626;

	// Triggers when entering the main menu screen
	__declspec(naked) void GameStartObserver()
	{
		static bool gameStartHandled = false;

		__asm
		{
			cmp byte ptr gameStartHandled, 0x1
			je conclusion // already handled

			push eax

			mov byte ptr gameStartHandled, 0x1
			call OnGameStartUpdates

			pop eax

			conclusion:
			// Execute original code and resume
			add esp, 0x4
			mov dword ptr [esp], eax

			jmp dword ptr gameStartObserverExit
		}
	}



	constexpr address worldLoadObserverEntrance = 0x66296C;
	constexpr address worldLoadObserverExit     = 0x662971;

	// Triggers during world loading screens
	__declspec(naked) void WorldLoadObserver()
	{
		__asm
		{
			push eax

			call OnWorldLoadUpdates

			pop eax

			// Execute original code and resume
			push edi
			xor edi, edi
			cmp eax, edi

			jmp dword ptr worldLoadObserverExit
		}
	}



	constexpr address playerConstructorEntrance = 0x43F005;
	constexpr address playerConstructorExit     = 0x43F00F;

	// Stores the player's AIPerpVehicle
	__declspec(naked) void PlayerConstructor()
	{
		__asm
		{
			lea eax, dword ptr [esi + 0x758]
			mov dword ptr [eax], 0x892988

			cmp dword ptr Globals::playerPerpVehicle, 0x0
			jne conclusion // vehicle already stored
	
			mov dword ptr Globals::playerPerpVehicle, eax

			conclusion:
			jmp dword ptr playerConstructorExit
		}
	}



	constexpr address playerDestructorEntrance = 0x43506C;
	constexpr address playerDestructorExit     = 0x435073;

	// Clears the player's AIPerpVehicle
	__declspec(naked) void PlayerDestructor()
	{
		__asm
		{
			cmp eax, dword ptr Globals::playerPerpVehicle
			jne conclusion // not stored vehicle

			xor ecx, ecx

			mov dword ptr Globals::playerPerpVehicle, ecx
			mov byte ptr Globals::playerHeatLevelKnown, cl

			mov dword ptr playerHeatLevel, ecx
			mov byte ptr playerIsRacing, cl

			conclusion:
			// Execute original code and resume
			mov edx, dword ptr [eax]
			mov ecx, eax
			call dword ptr [edx + 0x4]

			jmp dword ptr playerDestructorExit
		}
	}



	constexpr address gameplayObserverEntrance = 0x71D09D;
	constexpr address gameplayObserverExit     = 0x71D0A5;

	// Triggers during gameplay, but not every frame
	__declspec(naked) void GameplayObserver()
	{
		__asm
		{
			call OnGameplayUpdates

			// Execute original and resume
			mov edx, dword ptr [ebx]
			mov ecx, ebx
			mov dword ptr [esp + 0x14], ebx

			jmp dword ptr gameplayObserverExit
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

			mov byte ptr [esi + 0x769], al // "CopDetection.h" & "HelicopterVision.h"
			mov byte ptr [esi + 0x76A], al // "CopDetection.h" 
			mov byte ptr [esi + 0x76B], al // "CopSpawnOverrides.h"
			mov byte ptr [esi + 0x77B], al // "GroundSupports.h"

			jmp dword ptr resetInternalsExit
		}
	}



	constexpr address retryObserverEntrance = 0x6F4768;
	constexpr address retryObserverExit     = 0x6F476D;

	// Triggers on event / race resets
	__declspec(naked) void RetryObserver()
	{
		__asm
		{
			push edi
			mov edi, ecx
			push eax

			call OnRetryUpdates

			pop eax
			cmp eax, ebx

			jmp dword ptr retryObserverExit
		}
	}



	constexpr address heatEqualiserEntrance = 0x409084;
	constexpr address heatEqualiserExit     = 0x40908A;

	// Non-player drivers have the same Heat as the player
	__declspec(naked) void HeatEqualiser()
	{
		__asm
		{
			mov edi, eax

			mov eax, dword ptr Globals::playerPerpVehicle
			lea ecx, dword ptr [esp + 0x24]
			lea edx, dword ptr [eax + 0x1C]

			test eax, eax
			cmove edx, ecx // player vehicle unknown

			cmp eax, esi
			cmove edx, ecx // is player vehicle

			fld dword ptr [edx]

			jmp dword ptr heatEqualiserExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		// Code modifications 
		MemoryTools::MakeRangeJMP(HeatLevelObserver, heatLevelObserverEntrance, heatLevelObserverExit);
		MemoryTools::MakeRangeJMP(GameStartObserver, gameStartObserverEntrance, gameStartObserverExit);
		MemoryTools::MakeRangeJMP(WorldLoadObserver, worldLoadObserverEntrance, worldLoadObserverExit);
		MemoryTools::MakeRangeJMP(PlayerConstructor, playerConstructorEntrance, playerConstructorExit);
		MemoryTools::MakeRangeJMP(PlayerDestructor,  playerDestructorEntrance,  playerDestructorExit);
		MemoryTools::MakeRangeJMP(GameplayObserver,  gameplayObserverEntrance,  gameplayObserverExit);
		MemoryTools::MakeRangeJMP(ResetInternals,    resetInternalsEntrance,    resetInternalsExit);
		MemoryTools::MakeRangeJMP(RetryObserver,     retryObserverEntrance,     retryObserverExit);
		MemoryTools::MakeRangeJMP(HeatEqualiser,     heatEqualiserEntrance,     heatEqualiserExit);

		// Status flag
		featureEnabled = true;

		return true;
	}
}