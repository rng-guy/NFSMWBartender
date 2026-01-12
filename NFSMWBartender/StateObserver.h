#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "DestructionStrings.h"
#include "HelicopterVision.h"
#include "InteractiveMusic.h"
#include "CopDetection.h"
#include "RadioChatter.h"

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

			RadioChatter   ::SetToHeat(playerIsRacing, playerHeatLevel);
			GeneralSettings::SetToHeat(playerIsRacing, playerHeatLevel);
			GroundSupports ::SetToHeat(playerIsRacing, playerHeatLevel);
			PursuitObserver::SetToHeat(playerIsRacing, playerHeatLevel);
		}
		else if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("WARNING: [STA] Invalid Heat level", static_cast<int>(playerHeatLevel), (playerIsRacing) ? "(race)" : "(free-roam)");
	}





	// Hooking functions ----------------------------------------------------------------------------------------------------------------------------

	address onGameLoadValidationOriginal = 0x0;

	void __cdecl OnGameLoadValidation
	(
		const size_t  numArgs,
		const address argArray
	) {
		// The vanilla game calls "InitializeEverything" at this location
		const auto OriginalFunction = reinterpret_cast<void (__cdecl*)(size_t, address)>(onGameLoadValidationOriginal);

		// Call original function first
		OriginalFunction(numArgs, argArray);

		// Apply hooked logic last
		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Open("BartenderLog.txt");

			Globals::logger.Log          ("\n SESSION [VER] Bartender v2.10.00");
			Globals::logger.LogLongIndent("Basic    feature set", (Globals::basicSetEnabled) ? "enabled" : "disabled");
			Globals::logger.LogLongIndent("Advanced feature set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");

			if (MemoryTools::numRangeErrors > 0)
				Globals::logger.LogLongIndent("[MEM]", static_cast<int>(MemoryTools::numRangeErrors), "RANGE ERRORS!");

			if (MemoryTools::numCaveErrors > 0)
				Globals::logger.LogLongIndent("[MEM]", static_cast<int>(MemoryTools::numCaveErrors), "CAVE ERRORS!");

			if (MemoryTools::numHookErrors > 0)
				Globals::logger.LogLongIndent("[MEM]", static_cast<int>(MemoryTools::numHookErrors), "HOOK ERRORS!");
		}

		DestructionStrings::Validate();

		if constexpr (Globals::loggingEnabled)
		{
			HelicopterVision::LogSetupReport();
			InteractiveMusic::LogSetupReport();
		}

		CopDetection::Validate();
		RadioChatter::Validate();

		GeneralSettings::Validate();
		GroundSupports ::Validate();
		PursuitObserver::Validate();
	}



	address onGameplayUpdatesOriginal = 0x0;

	void __fastcall OnGameplayUpdates(const address soundAI)
	{
		// The vanilla game calls "SoundAI::SyncPursuit" at this location
		const auto OriginalFunction = reinterpret_cast<void (__thiscall*)(address)>(onGameplayUpdatesOriginal);

		// Apply hooked logic fist
		const auto IsRacing = reinterpret_cast<bool (__thiscall*)(address)>(0x409500);

		if (Globals::playerPerpVehicle and (playerIsRacing != IsRacing(Globals::playerPerpVehicle)))
		{
			playerIsRacing = (not playerIsRacing);

			OnHeatLevelUpdates();
		}

		PursuitObserver::UpdateState();

		// Call original function last
		OriginalFunction(soundAI);
	}



	address onWorldLoadUpdatesOriginal = 0x0;

	void OnWorldLoadUpdates()
	{
		// The vanilla game calls "nullsub_174" at this location
		const auto OriginalFunction = reinterpret_cast<void (*)()>(onWorldLoadUpdatesOriginal);

		// Apply hooked logic fist
		CopSpawnOverrides::ResetState();

		// Call original function last
		OriginalFunction();
	}



	address onRestartUpdatesOriginal = 0x0;

	void OnRestartUpdates()
	{
		// The vanilla game calls "World_RestoreProps" at this location
		const auto OriginalFunction = reinterpret_cast<void (*)()>(onRestartUpdatesOriginal);

		// Apply hooked logic fist
		playerHeatLevel = 0;

		Globals::playerHeatLevelKnown = false;

		CopSpawnOverrides::ResetState();

		// Call original function last
		OriginalFunction();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatEqualiserEntrance = 0x409084;
	constexpr address heatEqualiserExit     = 0x40908A;

	// Non-player drivers have the same Heat as the player
	__declspec(naked) void HeatEqualiser()
	{
		__asm
		{
			mov edi, eax

			mov edx, dword ptr Globals::playerPerpVehicle
			test edx, edx
			je conclusion // player vehicle unknown

			cmp edx, esi
			je conclusion // is player vehicle

			mov eax, dword ptr [edx + 0x1C] // player Heat
			mov dword ptr [esp + 0x24], eax // new perp Heat

			conclusion:
			// Execute original code and resume
			fld dword ptr [esp + 0x24] // new perp Heat

			jmp dword ptr heatEqualiserExit
		}
	}



	constexpr address resetAIVehicleEntrance = 0x414D6C;
	constexpr address resetAIVehicleExit     = 0x414D72;

	// Resets recycled padding bytes of AIPursuit objects
	__declspec(naked) void ResetAIVehicle()
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x80], bl

			mov byte ptr [esi + 0x81], bl // "CopDetection.h"
			mov byte ptr [esi + 0x82], bl // "CopDetection.h"
			mov byte ptr [esi + 0x83], bl // "GroundSupports.h"

			jmp dword ptr resetAIVehicleExit
		}
	}



	constexpr address gameStateUpdateEntrance = 0x6F6D6D;
	constexpr address gameStateUpdateExit     = 0x6F6D75;

	// Triggers on event / race resets
	__declspec(naked) void GameStateUpdate()
	{
		static uint32_t ticksOnPause = 0;

		__asm
		{
			cmp eax, dword ptr [esi + 0x2C] // game state
			je conclusion                   // state unchanged

			mov edx, Globals::gameTicks
			mov ecx, dword ptr [edx]

			mov edx, dword ptr [esi + 0x2C]
			mov dword ptr [esi + 0x2C], eax

			cmp eax, 0x4
			je paused // game now paused

			cmp edx, 0x4
			jne conclusion // game not just unpaused

			sub ecx, dword ptr ticksOnPause
			add dword ptr Globals::pausedTicks, ecx
			jmp conclusion // game was just unpaused

			paused:
			mov dword ptr ticksOnPause, ecx

			conclusion:
			jmp dword ptr gameStateUpdateExit
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
			fld dword ptr [eax + 0x1C] // perp Heat

			jmp dword ptr playerDestructorExit
		}
	}



	constexpr address heatLevelObserverEntrance = 0x4090BE;
	constexpr address heatLevelObserverExit     = 0x4090C6;

	// Triggers on Heat-level changes for the player
	__declspec(naked) void HeatLevelObserver()
	{
		__asm
		{
			cmp ebp, dword ptr playerHeatLevel
			je conclusion // Heat unchanged

			cmp esi, dword ptr Globals::playerPerpVehicle
			jne conclusion // not player vehicle

			cmp dword ptr [esp + 0x20], 0x416A75 // caller
			je conclusion                        // not true Heat

			mov dword ptr playerHeatLevel, ebp
			call OnHeatLevelUpdates

			conclusion:
			// Execute original code and resume
			cmp bl, byte ptr [esi + 0x2E] // previous race status
			setne al
			cmp ebp, edi

			jmp dword ptr heatLevelObserverExit
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



	constexpr address resetAIVehiclePursuitEntrance = 0x416B7A;
	constexpr address resetAIVehiclePursuitExit     = 0x416B80;

	// Resets recycled padding bytes of AIVehiclePursuit objects
	__declspec(naked) void ResetAIVehiclePursuit()
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x768], al

			mov byte ptr [esi + 0x769], al // "HelicopterVision.h"
			mov byte ptr [esi + 0x76A], al // "HeatChangeOverrides.h"
			mov byte ptr [esi + 0x76B], al // "CopSpawnOverrides.h"

			jmp dword ptr resetAIVehiclePursuitExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		// Code modifications 
		onGameLoadValidationOriginal = MemoryTools::MakeHook(OnGameLoadValidation, 0x6665B4);
		onGameplayUpdatesOriginal    = MemoryTools::MakeHook(OnGameplayUpdates,    0x721609);
		onWorldLoadUpdatesOriginal   = MemoryTools::MakeHook(OnWorldLoadUpdates,   0x662ADC);
		onRestartUpdatesOriginal     = MemoryTools::MakeHook(OnRestartUpdates,     0x63090B);

		MemoryTools::MakeRangeJMP(HeatEqualiser,         heatEqualiserEntrance,         heatEqualiserExit);
		MemoryTools::MakeRangeJMP(ResetAIVehicle,        resetAIVehicleEntrance,        resetAIVehicleExit);
		MemoryTools::MakeRangeJMP(GameStateUpdate,       gameStateUpdateEntrance,       gameStateUpdateExit);
		MemoryTools::MakeRangeJMP(PlayerDestructor,      playerDestructorEntrance,      playerDestructorExit);
		MemoryTools::MakeRangeJMP(HeatLevelObserver,     heatLevelObserverEntrance,     heatLevelObserverExit);
		MemoryTools::MakeRangeJMP(PlayerConstructor,     playerConstructorEntrance,     playerConstructorExit);
		MemoryTools::MakeRangeJMP(ResetAIVehiclePursuit, resetAIVehiclePursuitEntrance, resetAIVehiclePursuitExit);
		
		// Status flag
		featureEnabled = true;

		return true;
	}
}