#pragma once

#include <Windows.h>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"

#include "DestructionStrings.h"
#include "GroundSupport.h"
#include "Miscellaneous.h"
#include "PursuitBar.h"

#include "PursuitObserver.h"



namespace StateObserver
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	size_t  currentHeatLevel  = 0;
	address playerPerpVehicle = 0x0;
	bool    playerIsRacing    = false;
	bool    gameStartHandled  = false;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void OnHeatLevelUpdates()
	{
		// Shouldn't be necessary, but this is a BlackBox game..
		if ((currentHeatLevel >= 1) and (currentHeatLevel <= Globals::maxHeatLevel))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log("-------- [STA] Heat level now", (int)currentHeatLevel, (playerIsRacing) ? "(race)" : "(free-roam)");

			GroundSupport::SetToHeat(currentHeatLevel, playerIsRacing);
			Miscellaneous::SetToHeat(currentHeatLevel, playerIsRacing);
			PursuitBar::SetToHeat(currentHeatLevel, playerIsRacing);
			PursuitObserver::SetToHeat(currentHeatLevel, playerIsRacing);
		}
	}


	void OnGameStartUpdates()
	{
		if constexpr (Globals::loggingEnabled) 
			Globals::OpenLog();

		DestructionStrings::VerifyBinaryKeys();
	}


	void OnGameplayUpdates()
	{
		static bool (__thiscall* const IsRacing)(address) = (bool (__thiscall*)(address))0x409500;

		if (playerPerpVehicle and (playerIsRacing xor IsRacing(playerPerpVehicle)))
		{
			playerIsRacing = (not playerIsRacing);
			OnHeatLevelUpdates();
		}
	
		PursuitObserver::UpdateState();
	}


	void OnWorldLoadUpdates()
	{
		currentHeatLevel  = 0;
		playerPerpVehicle = 0x0;
		playerIsRacing    = false;

		PursuitObserver::HardResetState();
	}


	void OnRetryUpdates() 
	{
		currentHeatLevel = 0;

		PursuitObserver::SoftResetState();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatLevelObserverEntrance = 0x409091;
	constexpr address heatLevelObserverExit     = 0x409096;

	// Active when loading into areas or during pursuits
	__declspec(naked) void HeatLevelObserver()
	{
		__asm
		{
			test ebx, ebx // from HeatEqualiser
			je conclusion // player vehicle unknown

			cmp esi, ebx
			jne conclusion // not player vehicle

			mov ebx, [esp + 0x20] // return address

			cmp ebx, 0x443DEA // pursuit update
			je update

			cmp ebx, 0x423EC9 // pursuit initialisation
			je update

			cmp ebx, 0x60DE53 // race initialisation
			je update

			cmp ebx, 0x6F4BE5 // vehicle initialisation
			je update

			jmp conclusion // argument is not true Heat

			update:
			cmp eax, currentHeatLevel
			je conclusion // Heat is unchanged

			push ecx
			mov currentHeatLevel, eax
			call OnHeatLevelUpdates
			pop ecx

			conclusion:
			// Execute original code and resume
			mov eax, 0x91E000
			mov eax, [eax]

			jmp dword ptr heatLevelObserverExit
		}
	}



	constexpr address gameStartObserverEntrance = 0x570620;
	constexpr address gameStartObserverExit     = 0x570626;

	// Active when entering the main menu screen
	__declspec(naked) void GameStartObserver()
	{
		__asm
		{
			cmp byte ptr gameStartHandled, 0x1
			je conclusion // already handled

			push eax
			call OnGameStartUpdates
			mov byte ptr gameStartHandled, 0x1
			pop eax

			conclusion:
			// Execute original code and resume
			add esp, 0x4
			mov [esp], eax

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



	constexpr address gameplayObserverEntrance = 0x71D085;
	constexpr address gameplayObserverExit     = 0x71D08A;

	// Constantly active during gameplay, but not excessively so
	__declspec(naked) void GameplayObserver()
	{
		__asm
		{
			push eax
			push ecx

			call OnGameplayUpdates

			pop ecx
			pop eax

			// Execute original and resume
			sub esp, 0x18
			test eax, eax

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
			// Execute original code first
			push edi
			mov edi, ecx

			push eax
			call OnRetryUpdates
			pop eax

			// Execute original code and resume
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
			mov ebx, playerPerpVehicle

			test ebx, ebx
			je conclusion // player vehicle unknown

			cmp esi, ebx
			je conclusion // is player vehicle

			mov edx, [ebx + 0x1C] // player Heat
			mov [esp + 0x24], edx

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
			mov ebx, [esp + 0x3C] // return address
			cmp ebx, 0x43EF99
			jne conclusion        // not player vehicle

			lea ebx, [esi + 0x758]
			mov playerPerpVehicle, ebx

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

	bool Initialise(ConfigParser::Parser& parser)
	{
		MemoryEditor::DigCodeCave(&HeatLevelObserver, heatLevelObserverEntrance, heatLevelObserverExit);
		MemoryEditor::DigCodeCave(&GameStartObserver, gameStartObserverEntrance, gameStartObserverExit);
		MemoryEditor::DigCodeCave(&WorldLoadObserver, worldLoadObserverEntrance, worldLoadObserverExit);
		MemoryEditor::DigCodeCave(&GameplayObserver,  gameplayObserverEntrance,  gameplayObserverExit);
		MemoryEditor::DigCodeCave(&RetryObserver,     retryObserverEntrance,     retryObserverExit);
		MemoryEditor::DigCodeCave(&HeatEqualiser,     heatEqualiserEntrance,     heatEqualiserExit);
		MemoryEditor::DigCodeCave(&PerpVehicle,       perpVehicleEntrance,       perpVehicleExit);

		return true;
	}
}