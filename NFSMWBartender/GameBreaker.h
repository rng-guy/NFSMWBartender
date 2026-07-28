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

	constinit HeatParameters::Value<bool> canGainWhenActive  (true);
	constinit HeatParameters::Value<bool> canGainWhenInactive(true);

	constinit HeatParameters::Value<bool> canLoseWhenActive  (true);
	constinit HeatParameters::Value<bool> canLoseWhenInactive(true);

	// Code caves
	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<float> copTypeToBreakerChange(0.f); // seconds





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] address GetLocalPlayer()
	{
		const size_t numPlayers = AsVolatile<size_t>(0x92D884);
		return (numPlayers > 0) ? *AsVolatile<volatile address*>(0x92D87C) : 0x0;
	}



	void __stdcall ChargeSpeedbreaker(const float amount)
	{
		if (amount == 0.f) return;

		const address localPlayer = GetLocalPlayer();
		if (not localPlayer) return; // should never happen

		const bool isBreakerActive = AsVolatile<bool>(localPlayer + 0x34);

		if (amount > 0.f)
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
			Globals::logger.Log<1>("[GBR] Speedbreaker change:", amount);

		const auto  ChargeGameBreaker = AsFunction <void __thiscall (address, float)>(0x6F8F60);
		const float timeToRatio       = *AsVolatile<volatile float*>                 (0x6EDDC3);

		ChargeGameBreaker(localPlayer, timeToRatio * amount);
	}



	[[nodiscard]] bool IsPlayerInPursuit()
	{
		const address playerAIVehicle = Globals::GetAIVehicle(Globals::GetPlayerVehicle());
		return (playerAIVehicle and AsVolatile<address>(playerAIVehicle + 0x70));
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address wreckChangeEntrance = 0x418F9F;
	constexpr address wreckChangeExit     = 0x418FA5;

	// Updates speedbreaker charge on cop destruction
	__declspec(naked) void WreckChange()
	{
		__asm
		{
			mov ecx, esi
			call Globals::IsPlayerPursuit
			test al, al
			je conclusion // not player pursuit

			fld dword ptr [copWreckBreakerChange.current]
			fmul dword ptr [Globals::floatScale]

			push dword ptr [esi + 0xF8] // copType
			mov ecx, offset copTypeToBreakerChange
			call ModContainers::DefaultVaultMap<float>::GetValue
			fmul dword ptr [Globals::floatScale]

			faddp st(1), st(0)

			push eax
			fstp dword ptr [esp] // amount
			call ChargeSpeedbreaker

			conclusion:
			// Execute original code and resume
			fld dword ptr [esi + 0xEC]

			jmp dword ptr [wreckChangeExit]
		}
	}



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

			call IsPlayerInPursuit
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

			call IsPlayerInPursuit
			test al, al

			conclusion:
			jmp dword ptr [passiveRechargeExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseSpeedbreakerChanges(const HeatParameters::Parser& parser)
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
		HeatParameters::Parse(parser, "Speedbreaker:Wrecking",  copWreckBreakerChange);

		HeatParameters::Parse(parser, "Wrecking:Gains",  canGainWhenActive, canGainWhenInactive);
		HeatParameters::Parse(parser, "Wrecking:Losses", canLoseWhenActive, canLoseWhenInactive);

		// Speedbreaker changes
		ParseSpeedbreakerChanges(parser);

		// Code changes
		MemoryTools::MakeRangeJMP<wreckChangeEntrance,     wreckChangeExit>    (WreckChange);
		MemoryTools::MakeRangeJMP<driftRechargeEntrance,   driftRechargeExit>  (DriftRecharge);
		MemoryTools::MakeRangeJMP<passiveRechargeEntrance, passiveRechargeExit>(PassiveRecharge);

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

		canGainWhenActive  .SetToHeatState(isRacing, heatLevel);
		canGainWhenInactive.SetToHeatState(isRacing, heatLevel);

		canLoseWhenActive  .SetToHeatState(isRacing, heatLevel);
		canLoseWhenInactive.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}
}