#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "DestructionStrings.h"
#include "InteractiveMusic.h"
#include "RadioCallsigns.h"
#include "GroundSupport.h"
#include "GeneralSettings.h"

#include "PursuitObserver.h"



namespace StateObserver
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves
	address playerVehicle  = 0x0;
	bool    playerIsRacing = false;

	size_t currentHeatLevel = 0;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void OnHeatLevelUpdates()
	{
		// Shouldn't be necessary, but this is a BlackBox game..
		if ((currentHeatLevel >= 1) and (currentHeatLevel <= HeatParameters::maxHeatLevel))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("    HEAT [STA] Heat level is now", (int)currentHeatLevel, (playerIsRacing) ? "(race)" : "(free-roam)");

			GroundSupport   ::SetToHeat(playerIsRacing, currentHeatLevel);
			GeneralSettings ::SetToHeat(playerIsRacing, currentHeatLevel);
			PursuitObserver ::SetToHeat(playerIsRacing, currentHeatLevel);
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [STA] Invalid Heat level", (int)currentHeatLevel);
	}



	void OnGameStartUpdates()
	{
		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Open("BartenderLog.txt");
			Globals::logger.Log("\n SESSION [VER] Bartender v2.01.01");

			Globals::logger.LogLongIndent("Basic feature set",    (Globals::basicSetEnabled)    ? "enabled" : "disabled");
			Globals::logger.LogLongIndent("Advanced feature set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");
		}

		DestructionStrings::Validate();

		if constexpr (Globals::loggingEnabled)
			InteractiveMusic::LogConfigurationReport();

		RadioCallsigns ::Validate();
		GroundSupport  ::Validate();
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
		playerVehicle  = 0x0;
		playerIsRacing = false;

		currentHeatLevel = 0;

		PursuitObserver::HardResetState();
	}



	void OnRetryUpdates() 
	{
		currentHeatLevel = 0;

		PursuitObserver::SoftResetState();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatLevelObserverEntrance = 0x4090BE;
	constexpr address heatLevelObserverExit     = 0x4090C6;

	// Active when loading into areas or during pursuits
	__declspec(naked) void HeatLevelObserver()
	{
		__asm
		{
			cmp ebp, dword ptr currentHeatLevel
			je conclusion // Heat is unchanged

			mov eax, dword ptr playerVehicle

			test eax, eax
			je conclusion // player vehicle unknown

			cmp eax, esi
			jne conclusion // not player vehicle

			mov eax, dword ptr [esp + 0x20]

			cmp eax, 0x443DEA // pursuit update
			je update

			cmp eax, 0x423EC9 // pursuit initialisation
			je update

			cmp eax, 0x60DE53 // race initialisation
			je update

			cmp eax, 0x6F4BE5 // vehicle initialisation
			jne conclusion    // argument is not true Heat

			update:
			mov dword ptr currentHeatLevel, ebp
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

	// Active when entering the main menu screen
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

	// Active during world loading screens
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



	constexpr address gameplayObserverEntrance = 0x71D09D;
	constexpr address gameplayObserverExit     = 0x71D0A5;

	// Constantly active during gameplay, but not excessively so
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

	// Active during event / race resets
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

	// Non-player drivers have the same heat as the player
	__declspec(naked) void HeatEqualiser()
	{
		__asm
		{
			mov ebx, dword ptr playerVehicle

			test ebx, ebx
			je conclusion // player vehicle unknown

			cmp esi, ebx
			je conclusion // is player vehicle

			mov edx, dword ptr [ebx + 0x1C] // player Heat
			mov dword ptr [esp + 0x24], edx

			conclusion:
			// Execute original code and resume
			fld dword ptr [esp + 0x24]
			mov edi, eax

			jmp dword ptr heatEqualiserExit
		}
	}



	constexpr address perpVehicleEntrance = 0x43D252;
	constexpr address perpVehicleExit     = 0x43D257;

	// Identifies the player's AIPerpVehicle
	__declspec(naked) void PerpVehicle()
	{
		__asm
		{
			mov ebx, dword ptr [esp + 0x3C]

			cmp ebx, 0x43EF99
			jne conclusion // not player vehicle

			lea ebx, dword ptr [esi + 0x758]
			mov dword ptr playerVehicle, ebx

			conclusion:
			// Execute original code and resume
			pop edi
			mov eax, esi
			pop esi
			pop ebx

			jmp dword ptr perpVehicleExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		// Code caves
		MemoryTools::DigCodeCave(HeatLevelObserver, heatLevelObserverEntrance, heatLevelObserverExit);
		MemoryTools::DigCodeCave(GameStartObserver, gameStartObserverEntrance, gameStartObserverExit);
		MemoryTools::DigCodeCave(WorldLoadObserver, worldLoadObserverEntrance, worldLoadObserverExit);
		MemoryTools::DigCodeCave(GameplayObserver,  gameplayObserverEntrance,  gameplayObserverExit);
		MemoryTools::DigCodeCave(RetryObserver,     retryObserverEntrance,     retryObserverExit);
		MemoryTools::DigCodeCave(HeatEqualiser,     heatEqualiserEntrance,     heatEqualiserExit);
		MemoryTools::DigCodeCave(PerpVehicle,       perpVehicleEntrance,       perpVehicleExit);

		// Status flag
		featureEnabled = true;

		return true;
	}
}