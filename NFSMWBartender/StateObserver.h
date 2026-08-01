#pragma once

#include <cstdint>
#include <algorithm>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "GameBreaker.h"
#include "RadioChatter.h"
#include "GroundSupport.h"
#include "GeneralSettings.h"

#include "PursuitObserver.h"
#include "CopSpawnOverrides.h"
#include "HeatChangeOverrides.h"



namespace StateObserver
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Code caves
	size_t playerHeatLevel = 0;
	bool   playerIsRacing  = false;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ProcessHeatStateUpdate()
	{
		const size_t safeHeatLevel = std::clamp<size_t>(playerHeatLevel, 1, HeatParameters::maxHeatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			if (safeHeatLevel != playerHeatLevel)
				Globals::logger.Log("WARNING: [STA] Heat level", DecFormat(playerHeatLevel), "out of range");

			Globals::logger.Log("    HEAT [STA] Heat level now", DecFormat(safeHeatLevel), (playerIsRacing) ? "(race)" : "(roam)");
		}

		// Update Heat-level flag
		Globals::playerHeatLevelKnown = true;

		// Update Heat parameters
		RadioChatter::SetToHeatState(playerIsRacing, safeHeatLevel);

		GeneralSettings::SetToHeatState(playerIsRacing, safeHeatLevel);
		GroundSuppport ::SetToHeatState(playerIsRacing, safeHeatLevel);
		GameBreaker    ::SetToHeatState(playerIsRacing, safeHeatLevel);
			
		PursuitObserver::SetToHeatState(playerIsRacing, safeHeatLevel);
	}



	void ProcessDamagedCop
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle
	) {
		GameBreaker::NotifyOfDamagedCop(pursuit, copVehicle, perpVehicle);

		HeatChangeOverrides::NotifyOfDamagedCop(pursuit, copVehicle, perpVehicle);
	}


	
	void ProcessAssaultedCop
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle,
		const bool    isFirstOffence
	) {
		GameBreaker::NotifyOfAssaultedCop(pursuit, copVehicle, perpVehicle, isFirstOffence);

		HeatChangeOverrides::NotifyOfAssaultedCop(pursuit, copVehicle, perpVehicle, isFirstOffence);
	}



	void __fastcall ProcessDestroyedCop
	(
		const address pursuit,
		const address copVehicle
	) {
		GameBreaker::NotifyOfDestroyedCop(pursuit, copVehicle);

		HeatChangeOverrides::NotifyOfDestroyedCop(pursuit, copVehicle);
	}



	[[nodiscard]] bool __stdcall ShouldCollisionTriggerInfraction
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle,
		const bool    racerAtFault
	) {
		const address copAIVehiclePursuit = Globals::GetAIVehiclePursuit(copVehicle);
		if (not copAIVehiclePursuit) return false; // should never happen

		// Process damaged cop vehicle
		bool& damagedByRacer = AsReference<bool>(copAIVehiclePursuit + 0xB);

		if (not damagedByRacer)
		{
			damagedByRacer = true;

			if (pursuit) // is perp's pursuit, not necessarily the cop's
			{
				const auto NotifyCopDamaged = AsFunction<void __thiscall (address, address)>(0x40AF40);
				NotifyCopDamaged(pursuit, copVehicle); // for "cops hit" tracking in perp pursuit

				ProcessDamagedCop(pursuit, copVehicle, perpVehicle);
			}
		}

		// Process assault by perp
		if (not racerAtFault) return false;

		bool& alreadyAssaulted = AsReference<bool>(copAIVehiclePursuit - 0x758 + 0x76A); // padding byte

		if (pursuit)
			ProcessAssaultedCop(pursuit, copVehicle, perpVehicle, (not alreadyAssaulted));
		
		alreadyAssaulted = true;

		return (pursuit and Globals::IsPlayerPursuit(pursuit));
	}



	void __fastcall ProcessGameStateUpdate
	(
		const int newStateID, 
		const int oldStateID
	) {
		if (newStateID == oldStateID) return;

		// Update tracker for paused game ticks
		static constinit uint32_t numTicksOnPaused = 0;

		if (newStateID == 4) // paused
		{
			numTicksOnPaused = Globals::numGameTicks;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<1>("[STA] Game paused");
		}
		else if (oldStateID == 4) // unpaused
		{
			Globals::numPausedTicks += Globals::numGameTicks - numTicksOnPaused;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<1>("[STA] Game unpaused");
		}
	}





	// Hooking functions ----------------------------------------------------------------------------------------------------------------------------

	address ProcessGameplayOriginal = 0x0;

	void __fastcall ProcessGameplay(const address soundAI)
	{
		const auto OriginalFunction = AsFunction<void __thiscall (address)>(ProcessGameplayOriginal);

		// Apply hooked logic fist
		const auto IsRacing = AsFunction<bool __thiscall (address)>(0x409500);

		if (Globals::playerPerpVehicle and (playerIsRacing != IsRacing(Globals::playerPerpVehicle)))
		{
			playerIsRacing = (not playerIsRacing);
			ProcessHeatStateUpdate();
		}

		PursuitObserver::UpdateFeatureState();

		// Call original function last
		OriginalFunction(soundAI);
	}



	address ProcessWorldLoadOriginal = 0x0;

	void ProcessWorldLoad()
	{
		const auto OriginalFunction = AsFunction<void ()>(ProcessWorldLoadOriginal);

		// Apply hooked logic fist
		CopSpawnOverrides::FullResetFeatureState();

		// Call original function last
		OriginalFunction();
	}



	address ProcessEventRestartOriginal = 0x0;

	void ProcessEventRestart()
	{
		const auto OriginalFunction = AsFunction<void ()>(ProcessEventRestartOriginal);

		// Apply hooked logic fist
		playerHeatLevel               = 0;
		Globals::playerHeatLevelKnown = false;

		CopSpawnOverrides::SoftResetFeatureState();

		// Call original function last
		OriginalFunction();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address copDestroyedEntrance = 0x418F30;
	constexpr address copDestroyedExit     = 0x418F3B;

	// Trigges whenever a cop is destroyed
	__declspec(naked) void CopDestroyed()
	{
		__asm
		{
			push esi
			mov esi, ecx

			mov edx, dword ptr [esp + 0x8]
			call ProcessDestroyedCop // ecx: pursuit; edx: copVehicle

			// Execute original code and resume
			cmp byte ptr [esi + 0xA8], 0

			jmp dword ptr [copDestroyedExit]
		}
	}



	constexpr address perpCollisionEntrance = 0x429C8B;
	constexpr address perpCollisionExit     = 0x429CBB;

	// Checks whether collisions with cops constitute assault
	__declspec(naked) void PerpCollision()
	{
		__asm
		{
			movzx eax, byte ptr [esp + 0x13]
			mov ebx, dword ptr [esp + 0x14]

			mov edx, dword ptr [esp + 0x18]
			lea edx, dword ptr [edx - 0x8]

			push eax // racerAtFault
			push edx // perpVehicle
			push edi // copVehicle
			push ebx // pursuit
			call ShouldCollisionTriggerInfraction
			test al, al

			jmp dword ptr [perpCollisionExit]
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

			mov edx, dword ptr [Globals::playerPerpVehicle]
			test edx, edx
			je conclusion // player vehicle unknown

			cmp edx, esi
			je conclusion // is player vehicle

			mov eax, dword ptr [edx + 0x1C] // player Heat
			mov dword ptr [esp + 0x24], eax // new perp Heat

			conclusion:
			// Execute original code and resume
			fld dword ptr [esp + 0x24] // new perp Heat

			jmp dword ptr [heatEqualiserExit]
		}
	}



	constexpr address resetAIVehicleEntrance = 0x414D6C;
	constexpr address resetAIVehicleExit     = 0x414D72;

	// Resets repurposed padding bytes of AIPursuit objects
	__declspec(naked) void ResetAIVehicle()
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x80], bl

			mov byte ptr [esi + 0x81], bl // "CopDetection.h"
			mov byte ptr [esi + 0x82], bl // "CopDetection.h"
			mov byte ptr [esi + 0x83], bl // "GroundSuppport.h"

			jmp dword ptr [resetAIVehicleExit]
		}
	}



	constexpr address gameStateUpdateEntrance = 0x6F6D6D;
	constexpr address gameStateUpdateExit     = 0x6F6D75;

	// Triggers on game-state updates (e.g. (un)pausing)
	__declspec(naked) void GameStateUpdate()
	{
		__asm
		{
			mov edx, dword ptr [esi + 0x2C]

			cmp eax, edx
			je conclusion // state unchanged

			mov dword ptr [esi + 0x2C], eax

			mov ecx, eax
			call ProcessGameStateUpdate // ecx: newState; edx: oldState

			conclusion:
			jmp dword ptr [gameStateUpdateExit]
		}
	}



	constexpr address playerDestructorEntrance = 0x43506C;
	constexpr address playerDestructorExit     = 0x435073;

	// Clears the player's AIPerpVehicle
	__declspec(naked) void PlayerDestructor()
	{
		__asm
		{
			cmp eax, dword ptr [Globals::playerPerpVehicle]
			jne conclusion // not stored vehicle

			xor ecx, ecx

			mov dword ptr [Globals::playerPerpVehicle], ecx
			mov byte ptr [Globals::playerHeatLevelKnown], cl

			mov dword ptr [playerHeatLevel], ecx
			mov byte ptr [playerIsRacing], cl

			conclusion:
			// Execute original code and resume
			fld dword ptr [eax + 0x1C] // perp Heat

			jmp dword ptr [playerDestructorExit]
		}
	}



	constexpr address heatLevelObserverEntrance = 0x4090BE;
	constexpr address heatLevelObserverExit     = 0x4090C6;

	// Triggers on Heat-level changes for the player
	__declspec(naked) void HeatLevelObserver()
	{
		__asm
		{
			cmp ebp, dword ptr [playerHeatLevel]
			je conclusion // Heat unchanged

			cmp esi, dword ptr [Globals::playerPerpVehicle]
			jne conclusion // not player vehicle

			cmp dword ptr [esp + 0x20], 0x416A75 // caller
			je conclusion                        // not true Heat

			mov dword ptr [playerHeatLevel], ebp
			call ProcessHeatStateUpdate

			conclusion:
			// Execute original code and resume
			cmp bl, byte ptr [esi + 0x2E] // previous race status
			setne al
			cmp ebp, edi

			jmp dword ptr [heatLevelObserverExit]
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

			cmp dword ptr [Globals::playerPerpVehicle], 0x0
			jne conclusion // vehicle already stored

			mov dword ptr [Globals::playerPerpVehicle], eax

			conclusion:
			jmp dword ptr [playerConstructorExit]
		}
	}



	constexpr address resetAIVehiclePursuitEntrance = 0x416B7A;
	constexpr address resetAIVehiclePursuitExit     = 0x416B80;

	// Resets repurposed padding bytes of AIVehiclePursuit objects
	__declspec(naked) void ResetAIVehiclePursuit()
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x768], al

			mov byte ptr [esi + 0x769], al // "HelicopterVision.h"
			mov byte ptr [esi + 0x76A], al // "StateObserver.h"
			mov byte ptr [esi + 0x76B], al // "CopSpawnOverrides.h"

			jmp dword ptr [resetAIVehiclePursuitExit]
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(const HeatParameters::Parser& parser)
	{
		// Code modifications 
		ProcessGameplayOriginal     = MemoryTools::ReplaceCall(0x721609, ProcessGameplay);     // SoundAI::SyncPursuit (0x720850)
		ProcessWorldLoadOriginal    = MemoryTools::ReplaceCall(0x662ADC, ProcessWorldLoad);    // nullsub_174          (0x6C39C0)
		ProcessEventRestartOriginal = MemoryTools::ReplaceCall(0x63090B, ProcessEventRestart); // World_RestoreProps   (0x74D320)

		MemoryTools::MakeRangeNOP<0x429C74, 0x429C7F>(); // first perp-damage check

		MemoryTools::MakeRangeJMP<copDestroyedEntrance,          copDestroyedExit>         (CopDestroyed);
		MemoryTools::MakeRangeJMP<perpCollisionEntrance,         perpCollisionExit>        (PerpCollision);
		MemoryTools::MakeRangeJMP<heatEqualiserEntrance,         heatEqualiserExit>        (HeatEqualiser);
		MemoryTools::MakeRangeJMP<resetAIVehicleEntrance,        resetAIVehicleExit>       (ResetAIVehicle);
		MemoryTools::MakeRangeJMP<gameStateUpdateEntrance,       gameStateUpdateExit>      (GameStateUpdate);
		MemoryTools::MakeRangeJMP<playerDestructorEntrance,      playerDestructorExit>     (PlayerDestructor);
		MemoryTools::MakeRangeJMP<heatLevelObserverEntrance,     heatLevelObserverExit>    (HeatLevelObserver);
		MemoryTools::MakeRangeJMP<playerConstructorEntrance,     playerConstructorExit>    (PlayerConstructor);
		MemoryTools::MakeRangeJMP<resetAIVehiclePursuitEntrance, resetAIVehiclePursuitExit>(ResetAIVehiclePursuit);
		
		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}