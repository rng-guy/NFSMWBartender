#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "DestructionStrings.h"
#include "InteractiveMusic.h"
#include "RadioCallsigns.h"
#include "GroundSupports.h"
#include "GeneralSettings.h"

#include "PursuitObserver.h"



namespace StateObserver
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves 
	address playerVehicle   = 0x0;
	bool    playerIsRacing  = false;
	size_t  playerHeatLevel = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void OnHeatLevelUpdates()
	{
		// Shouldn't be necessary, but this is a BlackBox game..
		if ((playerHeatLevel >= 1) and (playerHeatLevel <= HeatParameters::maxHeatLevel))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("    HEAT [STA] Heat level is now", (int)playerHeatLevel, (playerIsRacing) ? "(race)" : "(free-roam)");

			GroundSupports  ::SetToHeat(playerIsRacing, playerHeatLevel);
			GeneralSettings ::SetToHeat(playerIsRacing, playerHeatLevel);
			PursuitObserver ::SetToHeat(playerIsRacing, playerHeatLevel);
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [STA] Invalid Heat level", (int)playerHeatLevel, (playerIsRacing) ? "(race)" : "(free-roam)");
	}



	void OnGameStartUpdates()
	{
		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Open("BartenderLog.txt");
			Globals::logger.Log ("\n SESSION [VER] Bartender v2.02.01");

			Globals::logger.LogLongIndent("Basic    feature set", (Globals::basicSetEnabled)    ? "enabled" : "disabled");
			Globals::logger.LogLongIndent("Advanced feature set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");
		}

		DestructionStrings::Validate();

		if constexpr (Globals::loggingEnabled)
			InteractiveMusic::LogConfigurationReport();

		RadioCallsigns ::Validate();
		GroundSupports ::Validate();
		PursuitObserver::Validate();
	}



	void OnGameplayUpdates()
	{
		static const auto IsRacing = (bool (__thiscall*)(address))0x409500;

		if (playerVehicle and (playerIsRacing != IsRacing(playerVehicle)))
		{
			playerIsRacing = (not playerIsRacing);

			OnHeatLevelUpdates();
		}
		
		PursuitObserver::UpdateState();
	}



	void OnWorldLoadUpdates()
	{
		PursuitObserver::HardResetState();
	}



	void OnRetryUpdates()
	{
		playerHeatLevel = 0; // forces an update

		PursuitObserver::SoftResetState();
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

			cmp esi, dword ptr playerVehicle
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

			cmp dword ptr playerVehicle, 0x0
			jne conclusion // vehicle already stored
	
			mov dword ptr playerVehicle, eax

			conclusion:
			jmp dword ptr playerConstructorExit
		}
	}



	constexpr address playerDestructorEntrance = 0x43506E;
	constexpr address playerDestructorExit     = 0x435073;

	// Clears the player's AIPerpVehicle
	__declspec(naked) void PlayerDestructor()
	{
		__asm
		{
			cmp eax, dword ptr playerVehicle
			jne conclusion // not stored vehicle

			xor ecx, ecx

			mov dword ptr playerVehicle, ecx
			mov byte ptr playerIsRacing, cl
			mov dword ptr playerHeatLevel, ecx

			conclusion:
			// Execute original code and resume
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
			mov eax, dword ptr playerVehicle

			test eax, eax
			je retain // player vehicle unknown

			cmp eax, esi
			je retain // is player vehicle

			fld dword ptr [eax + 0x1C]
			jmp conclusion

			retain:
			fld dword ptr [esp + 0x24]

			conclusion:
			jmp dword ptr heatEqualiserExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		// Code modifications 
		MemoryTools::DigCodeCave(HeatLevelObserver, heatLevelObserverEntrance, heatLevelObserverExit);
		MemoryTools::DigCodeCave(GameStartObserver, gameStartObserverEntrance, gameStartObserverExit);
		MemoryTools::DigCodeCave(WorldLoadObserver, worldLoadObserverEntrance, worldLoadObserverExit);
		MemoryTools::DigCodeCave(PlayerConstructor, playerConstructorEntrance, playerConstructorExit);
		MemoryTools::DigCodeCave(PlayerDestructor,  playerDestructorEntrance,  playerDestructorExit);
		MemoryTools::DigCodeCave(GameplayObserver,  gameplayObserverEntrance,  gameplayObserverExit);
		MemoryTools::DigCodeCave(RetryObserver,     retryObserverEntrance,     retryObserverExit);
		MemoryTools::DigCodeCave(HeatEqualiser,     heatEqualiserEntrance,     heatEqualiserExit);

		// Status flag
		featureEnabled = true;

		return true;
	}
}