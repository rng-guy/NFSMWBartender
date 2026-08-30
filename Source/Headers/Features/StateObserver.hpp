#pragma once

#include <limits>

#include "../Common/Globals.hpp"
#include "../Common/ConfigParser.hpp"
#include "../Common/HeatParameters.hpp"

#include "../Utilities/MemoryTools.hpp"

#include "Basic/GameBreaker.hpp"
#include "Basic/NitrousCharge.hpp"
#include "Basic/RadioSpeech.hpp"
#include "Basic/GroundSupport.hpp"
#include "Basic/GeneralSettings.hpp"

#include "Advanced/PursuitObserver.hpp"
#include "Advanced/HeatChangeOverrides.hpp"



namespace StateObserver
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[STO]";
	constexpr Globals::LogLiteral logName = "StateObserver";

	// First player vehicle
	address playerPerpVehicle = 0x0;

	// Gameplay updates
	bool forceNextGameplayUpdate = true;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void __fastcall ProcessHeatStateUpdate
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		static constinit HeatParameters::HeatState oldState{.isRace = false, .level = 0};

		const HeatParameters::HeatState newState(isRacing, HeatParameters::ClampHeatLevel(heatLevel));
		if (newState == oldState) return; // state unchanged; skip redundant Heat-level update(s)

		if constexpr (Globals::loggingEnabled)
		{
			if (newState.level != heatLevel)
				Globals::LogWarning(logTag, "Heat level", Globals::LogDec(heatLevel), "out of range");

			Globals::LogHeat(logTag, "Heat level now", Globals::LogDec(newState.level), (isRacing) ? "(race)" : "(roam)");
		}

		// "Basic" feature set
		RadioSpeech    ::SetToHeatState(newState);
		GeneralSettings::SetToHeatState(newState);
		GroundSuppport ::SetToHeatState(newState);
		NitrousCharge  ::SetToHeatState(newState);
		GameBreaker    ::SetToHeatState(newState);
			
		// "Advanced" feature set
		PursuitObserver::SetToHeatState(newState);

		// State cache
		oldState = newState;
	}



	void ProcessTaggedCop
	(
		const address copVehicle, 
		const address perpVehicle
	) {
		NitrousCharge::NotifyOfTaggedCop(copVehicle, perpVehicle);
		GameBreaker  ::NotifyOfTaggedCop(copVehicle, perpVehicle);

		HeatChangeOverrides::NotifyOfTaggedCop(copVehicle, perpVehicle);
	}


	
	void ProcessAssaultedCop
	(
		const address copVehicle,
		const address perpVehicle,
		const byte    numCopAssaulted
	) {
		NitrousCharge::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
		GameBreaker  ::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);

		HeatChangeOverrides::NotifyOfAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
	}



	void __fastcall ProcessFinishedCollision(const address perpVehicle) 
	{
		NitrousCharge::NotifyOfFinishedCollision(perpVehicle);
		GameBreaker  ::NotifyOfFinishedCollision(perpVehicle);

		// HeatChangeOverride doesn't need a notification for this
	}



	void __fastcall ProcessDestroyedCop
	(
		const address pursuit,
		const address copVehicle
	) {
		NitrousCharge::NotifyOfDestroyedCop(pursuit, copVehicle);
		GameBreaker  ::NotifyOfDestroyedCop(pursuit, copVehicle);

		HeatChangeOverrides::NotifyOfDestroyedCop(pursuit, copVehicle);
	}



	[[nodiscard]] bool __stdcall ShouldCollisionTriggerInfraction
	(
		const address copVehicle,
		const address perpVehicle,
		const bool    perpAtFault
	) {
		const address copAIVehiclePursuit = Globals::GetAIVehiclePursuitOfVehicle(copVehicle);
		ASSERT_CONDITION_THEN_IF_FALSE(copAIVehiclePursuit, return false);

		const address pursuit = Globals::GetPursuitOfPerpVehicle(perpVehicle);

		// Process damaged cop vehicle
		bool& damagedByRacer = AsReference<bool>(copAIVehiclePursuit + 0xB);

		if (not damagedByRacer)
		{
			damagedByRacer = true;

			if (pursuit) // may not be cop's pursuit (vanilla behaviour)
			{
				const auto NotifyCopDamaged = AsFunction<void __thiscall (address, address)>(0x40AF40);

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(pursuit, logTag, copVehicle, "tagged");

				NotifyCopDamaged(pursuit,    copVehicle);
				ProcessTaggedCop(copVehicle, perpVehicle);
			}
		}

		// Process assault by perp
		if (not perpAtFault) return false;

		byte& numCopAssaulted = AsReference<byte>(copAIVehiclePursuit - 0x758 + 0x76A); // padding byte

		if (numCopAssaulted < std::numeric_limits<byte>::max())
			++numCopAssaulted; // tracked separately for each vehicle

		if (pursuit) // may not be cop's pursuit (vanilla behaviour)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, copVehicle, "assaults:", Globals::LogDec(numCopAssaulted));

			ProcessAssaultedCop(copVehicle, perpVehicle, numCopAssaulted);
		}

		return (pursuit and Globals::IsPlayerPursuit(pursuit));
	}



	void __fastcall ProcessGameStateUpdate
	(
		const int newStateID, 
		const int oldStateID
	) {
		const bool isPaused  = (oldStateID == 3);
		const bool wasPaused = (newStateID == 3);

		if (not (isPaused or wasPaused)) return;

		if (isPaused and wasPaused)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Invalid game-state update");

			ASSERT_UNREACHABLE_THEN(return);
		}

		Globals::isGameplayPaused = isPaused;

		if constexpr (Globals::loggingEnabled)
			Globals::LogTagged(logTag, "Gameplay", (isPaused) ? "paused" : "resumed");

		if (isPaused) 
			Globals::numGameTicksOnLastPause = Globals::numGameTicks;

		else Globals::numFullyPausedGameTicks += Globals::numGameTicks - Globals::numGameTicksOnLastPause;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address gameTicksEntrance = 0x65C73F;
	constexpr address gameTicksExit     = 0x65C746;

	// Checks game ticks for unsigned overflow
	__declspec(naked) void GameTicks()
	{
		__asm
		{
			mov edx, 0x925968 // loop counter

			add ecx, eax
			jnc conclusion // no overflow

			mov dword ptr [Globals::numFullyPausedGameTicks], 0
			mov dword ptr [Globals::numGameTicksOnLastPause], ecx

			conclusion:
			mov eax, dword ptr [edx]

			jmp dword ptr [gameTicksExit]
		}
	}



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
			mov ebx, dword ptr [esp + 0x14]

			movzx eax, byte ptr [esp + 0x13]
			mov edx, dword ptr [esp + 0x18]
			lea edx, dword ptr [edx - 0x8]

			push eax // racerAtFault
			push edx // perpVehicle
			push edi // copVehicle
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

			mov edx, dword ptr [playerPerpVehicle]
			test edx, edx
			je conclusion // player vehicle unknown

			cmp edx, esi
			je conclusion // is player vehicle

			mov eax, dword ptr [edx + 0x1C] // player Heat
			mov dword ptr [esp + 0x24], eax // rival Heat

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



	constexpr address collisionResultEntrance = 0x429EDA;
	constexpr address collisionResultExit     = 0x429EDF;

	// Triggers after every perp-collision processing
	__declspec(naked) void CollisionResult()
	{
		__asm
		{
			mov edx, dword ptr [esp + 0x18]

			lea ecx, dword ptr [edx - 0x8]
			call ProcessFinishedCollision // ecx: perpVehicle

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x40]
			pop edi

			jmp dword ptr [collisionResultExit]
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
			call ProcessGameStateUpdate // ecx: newStateID; edx: oldStateID

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
			cmp eax, dword ptr [playerPerpVehicle]
			jne conclusion // not stored player vehicle

			mov dword ptr [playerPerpVehicle], 0x0

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
			cmp esi, dword ptr [playerPerpVehicle]
			jne conclusion // not player vehicle

			mov edx, ebp // new Heat level

			cmp dword ptr [esp + 0x20], 0x416A75
			cmove edx, edi // is race preparation

			cmp edx, edi
			jne update // Heat level changed

			cmp bl, byte ptr [esi + 0x2E]
			je conclusion // race status unchanged

			update:
			movzx ecx, bl
			call ProcessHeatStateUpdate // cl: isRacing; edx: heatLevel

			conclusion:
			// Execute original code and resume
			cmp bl, byte ptr [esi + 0x2E]
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
		static constexpr address FloatToInt = 0x7C4B80;

		__asm
		{
			// Execute original code first
			lea eax, dword ptr [esi + 0x758]
			mov dword ptr [eax], 0x892988

			cmp dword ptr [playerPerpVehicle], 0x0
			jne conclusion // already stored

			mov dword ptr [playerPerpVehicle], eax

			push ecx

			movzx ebx, byte ptr [eax + 0x2E]

			fld dword ptr [eax + 0x1C]
			call dword ptr [FloatToInt]

			mov ecx, ebx
			mov edx, eax
			call ProcessHeatStateUpdate // cl: isRacing; edx: heatLevel

			pop ecx

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



	

	// Hooking functions ----------------------------------------------------------------------------------------------------------------------------

	address ProcessGameplayOriginal = 0x0;

	void __fastcall ProcessGameplay(const address simSystem)
	{
		static constinit float lastUpdateTimestamp = 0.f; // seconds

		// Call original function first (actually __thiscall with 0 arguments)
		AsFunction<decltype(ProcessGameplay)>(ProcessGameplayOriginal)(simSystem);

		// Check update timestamp
		const float timestamp = Globals::GetGameplayTime();

		constexpr float updateInterval = 1.f / 10.f; // seconds

		if ((not forceNextGameplayUpdate) and (timestamp < lastUpdateTimestamp + updateInterval))
		{
			// Guard against potential wrap-around / reset
			if (timestamp >= lastUpdateTimestamp) return;
		}

		// "Advanced" feature set
		PursuitObserver::NotifyOfGameplay();

		// Timestamp update
		lastUpdateTimestamp     = timestamp;
		forceNextGameplayUpdate = false;
	}



	address ProcessWorldLoadOriginal = 0x0;

	void ProcessWorldLoad()
	{
		// Apply hooked logic fist
		forceNextGameplayUpdate = true;

		PursuitObserver::NotifyOfHardEventReset();

		// Call original function last
		AsFunction<decltype(ProcessWorldLoad)>(ProcessWorldLoadOriginal)();
	}



	address ProcessEventRestartOriginal = 0x0;

	void ProcessEventRestart()
	{
		// Apply hooked logic fist
		forceNextGameplayUpdate = true;
		
		PursuitObserver::NotifyOfSoftEventReset();

		// Call original function last
		AsFunction<decltype(ProcessEventRestart)>(ProcessEventRestartOriginal)();
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(const ConfigParser::Parser& parser)
	{
		// Code modifications 
		MemoryTools::MakeRangeNOP<0x429C74, 0x429C7F>(); // first perp-damage check

		MemoryTools::MakeRangeJMP<gameTicksEntrance,             gameTicksExit>            (GameTicks);
		MemoryTools::MakeRangeJMP<copDestroyedEntrance,          copDestroyedExit>         (CopDestroyed);
		MemoryTools::MakeRangeJMP<perpCollisionEntrance,         perpCollisionExit>        (PerpCollision);
		MemoryTools::MakeRangeJMP<heatEqualiserEntrance,         heatEqualiserExit>        (HeatEqualiser);
		MemoryTools::MakeRangeJMP<resetAIVehicleEntrance,        resetAIVehicleExit>       (ResetAIVehicle);
		MemoryTools::MakeRangeJMP<collisionResultEntrance,       collisionResultExit>      (CollisionResult);
		MemoryTools::MakeRangeJMP<gameStateUpdateEntrance,       gameStateUpdateExit>      (GameStateUpdate);
		MemoryTools::MakeRangeJMP<playerDestructorEntrance,      playerDestructorExit>     (PlayerDestructor);
		MemoryTools::MakeRangeJMP<heatLevelObserverEntrance,     heatLevelObserverExit>    (HeatLevelObserver);
		MemoryTools::MakeRangeJMP<playerConstructorEntrance,     playerConstructorExit>    (PlayerConstructor);
		MemoryTools::MakeRangeJMP<resetAIVehiclePursuitEntrance, resetAIVehiclePursuitExit>(ResetAIVehiclePursuit);

		ProcessGameplayOriginal     = MemoryTools::ReplaceCall(0x6F6EE6, ProcessGameplay);     // SimSystem::UpdateFrame (0x6F6CF0)
		ProcessWorldLoadOriginal    = MemoryTools::ReplaceCall(0x662ADC, ProcessWorldLoad);    // nullsub_174            (0x6C39C0)
		ProcessEventRestartOriginal = MemoryTools::ReplaceCall(0x63090B, ProcessEventRestart); // World_RestoreProps     (0x74D320)
		
		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}