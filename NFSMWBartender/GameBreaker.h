#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "ParameterSets.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace GameBreaker
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[GBR]";
	constexpr LogLiteral logName = "GameBreaker";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, passiveRechargeEnabled, true);
	constinit HEAT_PARAMETER_VALUE(bool, driftRechargeEnabled,   true);

	constinit HEAT_PARAMETER_VALUE(bool, canGainWhenActive, true);
	constinit HEAT_PARAMETER_VALUE(bool, canLoseWhenActive, true);

	constinit HEAT_PARAMETER_VALUE(bool, canGainWhenInactive, true);
	constinit HEAT_PARAMETER_VALUE(bool, canLoseWhenInactive, true);

	// Parameter sets
	RELEASE_CONSTINIT ParameterSets::CopInteractions breakerInteractions; // seconds

	// Code caves
	float pendingCollisionBreakerChange = 0.f;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ChargeSpeedbreakerOfTarget
	(
		const address pursuit, 
		const float   seconds
	) {
		const address localPlayer = Globals::GetLocalPlayerOfPursuit(pursuit);
		if (not localPlayer) return; // not player pursuit

		const bool isBreakerActive = AsReference<bool>(localPlayer + 0x34);

		const auto& canGain = (isBreakerActive) ? canGainWhenActive : canGainWhenInactive;
		const auto& canLose = (isBreakerActive) ? canLoseWhenActive : canLoseWhenInactive;

		if ((seconds > 0.f) and (not canGain.current)) return;
		if ((seconds < 0.f) and (not canLose.current)) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogFull(pursuit, logTag, "Speedbreaker change:", seconds);

		const auto  ChargeGameBreaker = AsFunction  <void __thiscall (address, float)>(0x6F8F60);
		const float timeToRatio       = *AsReference<const float* const>              (0x6EDDC3);

		ChargeGameBreaker(localPlayer, Globals::floatScale * (timeToRatio * seconds));
	}



	[[nodiscard]] bool __fastcall IsInPursuit(const address localPlayer)
	{
		if (not localPlayer) return false; // should never happen

		for (const address pursuit : ModContainers::PursuitList())
			if (localPlayer == Globals::GetLocalPlayerOfPursuit(pursuit)) return true;

		return false;
	}



	void ProcessTaggedCop(const address copVehicle)
	{
		pendingCollisionBreakerChange += breakerInteractions.GetTaggingChange(copVehicle);
	}



	void ProcessAssaultedCop
	(
		const address copVehicle,
		const byte    numCopAssaulted
	) {
		pendingCollisionBreakerChange += breakerInteractions.GetAssaultChange(copVehicle, numCopAssaulted);
	}



	void ProcessFinishedCollision(const address perpVehicle)
	{
		if (pendingCollisionBreakerChange == 0.f) return;

		if (const address pursuit = Globals::GetPursuitOfPerpVehicle(perpVehicle))
			ChargeSpeedbreakerOfTarget(pursuit, pendingCollisionBreakerChange);

		pendingCollisionBreakerChange = 0.f;
	}



	void ProcessDestroyedCop
	(
		const address pursuit,
		const address copVehicle
	) {
		const float breakerChange = breakerInteractions.GetWreckingChange(copVehicle);
		if (breakerChange == 0.f) return;

		ChargeSpeedbreakerOfTarget(pursuit, breakerChange);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address driftRechargeEntrance = 0x6A99BC;
	constexpr address driftRechargeExit     = 0x6A99C1;

	// Toggles drift-based Speedbreaker recharging
	__declspec(naked) void DriftRecharge()
	{
		__asm
		{
			fnstsw ax
			test ah, 0x41
			jne conclusion // below speed threshold

			cmp byte ptr [driftRechargeEnabled.current], 1
			je conclusion // drift recharging unrestricted

			mov edx, dword ptr [esi + 0x34]

			mov ecx, dword ptr [edx + 0x58]
			call IsInPursuit // ecx: localPlayer
			test al, al

			conclusion:
			jmp dword ptr [driftRechargeExit]
		}
	}



	constexpr address passiveRechargeEntrance = 0x6EDDDE;
	constexpr address passiveRechargeExit     = 0x6EDDE3;

	// Toggles passive Speedbreaker recharging
	__declspec(naked) void PassiveRecharge()
	{
		__asm
		{
			fnstsw ax
			test ah, 0x41
			jne conclusion // below speed threshold

			cmp byte ptr [passiveRechargeEnabled.current], 1
			je conclusion // passive recharging unrestricted

			lea ecx, dword ptr [esi + 0x4C]
			call IsInPursuit // ecx: localPlayer
			test al, al

			conclusion:
			jmp dword ptr [passiveRechargeExit]
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Speedbreaker.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Speedbreaker:Mechanics", passiveRechargeEnabled, driftRechargeEnabled);

		HeatParameters::Parse(parser, "Speedbreaker:Active", canGainWhenActive, canLoseWhenActive);

		HeatParameters::Parse(parser, "Speedbreaker:Inactive", canGainWhenInactive, canLoseWhenInactive);

		// Parameter sets
		breakerInteractions.Parse(parser, "Speedbreaker");

		// Code changes
		MemoryTools::MakeRangeJMP<driftRechargeEntrance,   driftRechargeExit>  (DriftRecharge);
		MemoryTools::MakeRangeJMP<passiveRechargeEntrance, passiveRechargeExit>(PassiveRecharge);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		passiveRechargeEnabled.SetToHeatState(state);
		driftRechargeEnabled  .SetToHeatState(state);

		breakerInteractions.SetToHeatState(state);

		canGainWhenActive.SetToHeatState(state);
		canLoseWhenActive.SetToHeatState(state);

		canGainWhenInactive.SetToHeatState(state);
		canLoseWhenInactive.SetToHeatState(state);
	}



	void NotifyOfTaggedCop
	(
		const address copVehicle, 
		const address perpVehicle
	) {
		if (not anyFeatureEnabled) return;

		ProcessTaggedCop(copVehicle);
	}



	void NotifyOfAssaultedCop
	(
		const address copVehicle,
		const address perpVehicle,
		const byte    numCopAssaulted
	) {
		if (not anyFeatureEnabled) return;

		ProcessAssaultedCop(copVehicle, numCopAssaulted);
	}



	void NotifyOfFinishedCollision(const address perpVehicle)
	{
		if (not anyFeatureEnabled) return;

		ProcessFinishedCollision(perpVehicle);
	}



	void NotifyOfDestroyedCop
	(
		const address pursuit, 
		const address copVehicle
	) {
		if (not anyFeatureEnabled) return;

		ProcessDestroyedCop(pursuit, copVehicle);
	}
}