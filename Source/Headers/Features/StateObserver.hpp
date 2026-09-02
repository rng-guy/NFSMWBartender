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





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Checks game ticks for unsigned overflow to adjust mod-internal timer
	ASSEMBLY_DETOUR(GameTicks, /* begin = */ 0x65C73F, /* end = */ 0x65C746)
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

			EXIT_ASSEMBLY_DETOUR(GameTicks)
		}
	}



	// Trigges whenever a cop is destroyed
	ASSEMBLY_DETOUR(CopDestroyed, 0x418F30, 0x418F3B)
	{
		__asm
		{
			push esi
			mov esi, ecx

			mov edx, dword ptr [esp + 0x8]
			call ProcessDestroyedCop // ecx: pursuit; edx: copVehicle

			// Execute original code and resume
			cmp byte ptr [esi + 0xA8], 0

			EXIT_ASSEMBLY_DETOUR(CopDestroyed)
		}
	}



	// Checks whether collisions with cops constitute assault
	ASSEMBLY_DETOUR(PerpCollision, 0x429C8B, 0x429CBB)
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

			EXIT_ASSEMBLY_DETOUR(PerpCollision)
		}
	}



	// Non-player drivers have the same Heat as the player
	ASSEMBLY_DETOUR(HeatEqualiser, 0x409084, 0x40908A)
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

			EXIT_ASSEMBLY_DETOUR(HeatEqualiser)
		}
	}



	// Resets repurposed padding bytes of AIPursuit objects
	ASSEMBLY_DETOUR(ResetAIVehicle, 0x414D6C, 0x414D72)
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x80], bl

			mov byte ptr [esi + 0x81], bl // used in "CopDetection.hpp"
			mov byte ptr [esi + 0x82], bl // used in "CopDetection.hpp"
			mov byte ptr [esi + 0x83], bl // used in "GroundSuppport.hpp"

			EXIT_ASSEMBLY_DETOUR(ResetAIVehicle)
		}
	}



	// Triggers after every perp-collision processing
	ASSEMBLY_DETOUR(CollisionResult, 0x429EDA, 0x429EDF)
	{
		__asm
		{
			mov edx, dword ptr [esp + 0x18]

			lea ecx, dword ptr [edx - 0x8]
			call ProcessFinishedCollision // ecx: perpVehicle

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x40]
			pop edi

			EXIT_ASSEMBLY_DETOUR(CollisionResult)
		}
	}



	// Triggers on game-state updates (e.g. (un)pausing)
	ASSEMBLY_DETOUR(GameStateUpdate, 0x6F6D6D, 0x6F6D75)
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
			EXIT_ASSEMBLY_DETOUR(GameStateUpdate)
		}
	}



	// Clears the player's AIPerpVehicle
	ASSEMBLY_DETOUR(PlayerDestructor, 0x43506C, 0x435073)
	{
		__asm
		{
			cmp eax, dword ptr [playerPerpVehicle]
			jne conclusion // not stored player vehicle

			mov dword ptr [playerPerpVehicle], 0x0

			conclusion:
			// Execute original code and resume
			fld dword ptr [eax + 0x1C] // perp Heat

			EXIT_ASSEMBLY_DETOUR(PlayerDestructor)
		}
	}



	// Triggers on Heat-level changes for the player
	ASSEMBLY_DETOUR(HeatLevelObserver, 0x4090BE, 0x4090C6)
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

			EXIT_ASSEMBLY_DETOUR(HeatLevelObserver)
		}
	}



	// Stores the player's AIPerpVehicle
	ASSEMBLY_DETOUR(PlayerConstructor, 0x43F005, 0x43F00F)
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
			EXIT_ASSEMBLY_DETOUR(PlayerConstructor)
		}
	}



	// Resets repurposed padding bytes of AIVehiclePursuit objects
	ASSEMBLY_DETOUR(ResetAIVehiclePursuit, 0x416B7A, 0x416B80)
	{
		__asm
		{
			// Execute original code first
			mov byte ptr [esi + 0x768], al

			mov byte ptr [esi + 0x769], al // used in "HelicopterVision.hpp"
			mov byte ptr [esi + 0x76A], al // used in "StateObserver.hpp"
			mov byte ptr [esi + 0x76B], al // used in "CopSpawnOverrides.hpp"

			EXIT_ASSEMBLY_DETOUR(ResetAIVehiclePursuit)
		}
	}



	

	// Hook functions -------------------------------------------------------------------------------------------------------------------------------

	HOOK_ORIGINAL(ProcessGameplay);

	void __fastcall ProcessGameplay(const address simSystem)
	{
		static constinit float lastUpdateTimestamp = 0.f; // seconds

		CALL_HOOK_ORIGINAL(ProcessGameplay, simSystem); // actually __thiscall with 0 arguments

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



	HOOK_ORIGINAL(ProcessWorldLoad);

	void __cdecl ProcessWorldLoad()
	{
		// Apply hooked logic fist
		forceNextGameplayUpdate = true;

		PursuitObserver::NotifyOfHardEventReset();

		CALL_HOOK_ORIGINAL(ProcessWorldLoad);
	}



	HOOK_ORIGINAL(ProcessEventRestart);

	void __cdecl ProcessEventRestart()
	{
		// Apply hooked logic fist
		forceNextGameplayUpdate = true;
		
		PursuitObserver::NotifyOfSoftEventReset();

		CALL_HOOK_ORIGINAL(ProcessEventRestart);
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(const ConfigParser::Parser& parser)
	{
		// Code modifications 
		MemoryTools::MakeRangeNOP<0x429C74, 0x429C7F>(); // first perp-damage check

		PATCH_ASSEMBLY_DETOUR(GameTicks);
		PATCH_ASSEMBLY_DETOUR(CopDestroyed);
		PATCH_ASSEMBLY_DETOUR(PerpCollision);
		PATCH_ASSEMBLY_DETOUR(HeatEqualiser);
		PATCH_ASSEMBLY_DETOUR(ResetAIVehicle);
		PATCH_ASSEMBLY_DETOUR(CollisionResult);
		PATCH_ASSEMBLY_DETOUR(GameStateUpdate);
		PATCH_ASSEMBLY_DETOUR(PlayerDestructor);
		PATCH_ASSEMBLY_DETOUR(HeatLevelObserver);
		PATCH_ASSEMBLY_DETOUR(PlayerConstructor);
		PATCH_ASSEMBLY_DETOUR(ResetAIVehiclePursuit);

		PATCH_HOOK_FUNCTION(ProcessGameplay,     0x6F6EE6); // SimSystem::UpdateFrame (0x6F6CF0)
		PATCH_HOOK_FUNCTION(ProcessWorldLoad,    0x662ADC); // nullsub_174            (0x6C39C0)
		PATCH_HOOK_FUNCTION(ProcessEventRestart, 0x63090B); // World_RestoreProps     (0x74D320)
		
		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}