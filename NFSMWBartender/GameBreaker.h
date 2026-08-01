#pragma once

#include <vector>
#include <functional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace GameBreaker
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Value<bool> passiveRechargeEnabled(true);
	constinit HeatParameters::Value<bool> driftRechargeEnabled  (true);

	constinit HeatParameters::Value<float> copWreckBreakerChange(0.f); // seconds

	constinit HeatParameters::Value<float> copDamagedBreakerChange(0.f); // seconds

	constinit HeatParameters::Value<float> breakerChangePerAssault(0.f); // seconds
	constinit HeatParameters::Value<bool>  onlyOneAssaultPerCop   (true);

	constinit HeatParameters::Value<bool> canGainWhenActive  (true);
	constinit HeatParameters::Value<bool> canGainWhenInactive(true);

	constinit HeatParameters::Value<bool> canLoseWhenActive  (true);
	constinit HeatParameters::Value<bool> canLoseWhenInactive(true);

	// Code caves
	float pendingCollisionBreakerChange = 0.f;

	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<float> copTypeToBreakerChange(0.f); // seconds





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ChargeSpeedbreaker
	(
		const address localPlayer, 
		const float   amount
	) {
		const bool isBreakerActive = AsReference<bool>(localPlayer + 0x34);

		if (amount >= 0.f)
		{
			if (isBreakerActive       and (not canGainWhenActive  .current)) return;
			if ((not isBreakerActive) and (not canGainWhenInactive.current)) return;
		}
		else
		{
			if (isBreakerActive       and (not canLoseWhenActive  .current)) return;
			if ((not isBreakerActive) and (not canLoseWhenInactive.current)) return;
		}

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log(localPlayer, "[GBR] Speedbreaker change:", amount);

		const auto  ChargeGameBreaker = AsFunction  <void __thiscall (address, float)>(0x6F8F60);
		const float timeToRatio       = *AsReference<const float* const>              (0x6EDDC3);

		ChargeGameBreaker(localPlayer, Globals::floatScale * timeToRatio * amount);
	}



	void ProcessDamagedCop(const address pursuit) 
	{
		if (not Globals::IsPlayerPursuit(pursuit)) return;

		pendingCollisionBreakerChange += copDamagedBreakerChange.current;
	}



	void ProcessAssaultedCop
	(
		const address pursuit,
		const bool    isFirstOffence
	) {
		if (not Globals::IsPlayerPursuit(pursuit))                 return;
		if ((not isFirstOffence) and onlyOneAssaultPerCop.current) return;

		pendingCollisionBreakerChange += breakerChangePerAssault.current;
	}



	void ProcessDestroyedCop
	(
		const address pursuit,
		const address copVehicle
	) {
		const address localPlayer = Globals::GetLocalPlayerOfPursuit(pursuit);
		if (not localPlayer) return; // is non-player pursuit

		const vault copType       = Globals::GetVehicleType(copVehicle);
		const float breakerChange = copWreckBreakerChange.current + copTypeToBreakerChange.GetValue(copType);

		if (breakerChange != 0.f)
			ChargeSpeedbreaker(localPlayer, breakerChange);
	}



	void __fastcall ApplyPendingCollisionBreakerChange(const address perpVehicle)
	{
		if (pendingCollisionBreakerChange == 0.f) return;

		const address pursuit     = Globals::GetPursuitOfPerpVehicle(perpVehicle);
		const address localPlayer = Globals::GetLocalPlayerOfPursuit(pursuit);

		if (localPlayer)
			ChargeSpeedbreaker(localPlayer, pendingCollisionBreakerChange);
		
		pendingCollisionBreakerChange = 0.f;
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

			mov ecx, dword ptr [esi + 0x34]

			push dword ptr [ecx + 0x58] // localPlayer
			call Globals::IsPlayerInPursuit
			add esp, 0x4
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

			push ecx // localPlayer
			call Globals::IsPlayerInPursuit
			add esp, 0x4
			test al, al

			conclusion:
			jmp dword ptr [passiveRechargeExit]
		}
	}



	constexpr address collisionResultEntrance = 0x429EDA;
	constexpr address collisionResultExit     = 0x429EDF;

	// Applies the Speedbreaker changes from player collisions
	__declspec(naked) void CollisionResult()
	{
		__asm
		{
			mov edx, dword ptr [esp + 0x18]

			lea ecx, dword ptr [edx - 0x8]
			call ApplyPendingCollisionBreakerChange // ecx: perpVehicle
	
			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x40]
			pop edi

			jmp dword ptr [collisionResultExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseVehicleSpeedbreakerChanges(const HeatParameters::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<float>            changes;

		parser.ParseUser<std::string_view, float>("Wrecking:Vehicles", copNames, {changes});

		return copTypeToBreakerChange.FillFromVectors
		(
			"Vehicle-to-change",
			HeatParameters::configDefaultVaultHash,
			ModContainers::MapFillSetup(copNames, Globals::GetVaultHash, Globals::DoesVehicleTypeExist),
			ModContainers::MapFillSetup(changes,  std::identity{},       ModContainers::AlwaysValid{})
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [GBR] GameBreaker");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Speedbreaker.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Speedbreaker:Mechanics", passiveRechargeEnabled, driftRechargeEnabled);

		HeatParameters::Parse(parser, "Speedbreaker:Wrecking", copWreckBreakerChange);

		HeatParameters::Parse(parser, "Speedbreaker:Collisions", copDamagedBreakerChange);

		HeatParameters::Parse(parser, "Collisions:Assault", breakerChangePerAssault, onlyOneAssaultPerCop);

		HeatParameters::Parse(parser, "Speedbreaker:Gains", canGainWhenActive, canGainWhenInactive);

		HeatParameters::Parse(parser, "Speedbreaker:Losses", canLoseWhenActive, canLoseWhenInactive);

		// Vehicle-specific Speedbreaker changes
		ParseVehicleSpeedbreakerChanges(parser);

		// Code changes
		MemoryTools::MakeRangeJMP<driftRechargeEntrance,   driftRechargeExit>  (DriftRecharge);
		MemoryTools::MakeRangeJMP<passiveRechargeEntrance, passiveRechargeExit>(PassiveRecharge);
		MemoryTools::MakeRangeJMP<collisionResultEntrance, collisionResultExit>(CollisionResult);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		Globals::logger.Log("    HEAT [GBR] GameBreaker");

		passiveRechargeEnabled.Log("passiveRechargeEnabled  ");
		driftRechargeEnabled  .Log("driftRechargeEnabled    ");

		copWreckBreakerChange.Log("copWreckBreakerChange   ");

		copDamagedBreakerChange.Log("copDamagedBreakerChange ");

		breakerChangePerAssault.Log("breakerChangePerAssault ");
		onlyOneAssaultPerCop   .Log("onlyOneAssaultPerCop    ");

		canGainWhenActive  .Log("canGainWhenActive       ");
		canGainWhenInactive.Log("canGainWhenInactive     ");

		canLoseWhenActive  .Log("canLoseWhenActive       ");
		canLoseWhenInactive.Log("canLoseWhenInactive     ");
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		passiveRechargeEnabled.SetToHeatState(isRacing, heatLevel);
		driftRechargeEnabled  .SetToHeatState(isRacing, heatLevel);

		copWreckBreakerChange.SetToHeatState(isRacing, heatLevel);

		copDamagedBreakerChange.SetToHeatState(isRacing, heatLevel);

		breakerChangePerAssault.SetToHeatState(isRacing, heatLevel);
		onlyOneAssaultPerCop   .SetToHeatState(isRacing, heatLevel);

		canGainWhenActive  .SetToHeatState(isRacing, heatLevel);
		canGainWhenInactive.SetToHeatState(isRacing, heatLevel);

		canLoseWhenActive  .SetToHeatState(isRacing, heatLevel);
		canLoseWhenInactive.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}



	void NotifyOfDamagedCop
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle
	) {
		if (not anyFeatureEnabled) return;

		ProcessDamagedCop(pursuit);
	}



	void NotifyOfAssaultedCop
	(
		const address pursuit,
		const address copVehicle,
		const address perpVehicle,
		const bool    isFirstOffence
	) {
		if (not anyFeatureEnabled) return;

		ProcessAssaultedCop(pursuit, isFirstOffence);
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