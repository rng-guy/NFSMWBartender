#pragma once

#include <vector>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace GameBreaker
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Pair<bool> passiveRechargeEnableds(true);
	constinit HeatParameters::Pair<bool> driftRechargeEnableds  (true);

	constinit HeatParameters::Pair<float> copWreckBreakerChanges(0.f); // seconds

	constinit HeatParameters::Pair<bool> canGainWhenActives  (true);
	constinit HeatParameters::Pair<bool> canGainWhenInactives(true);

	constinit HeatParameters::Pair<bool> canLoseWhenActives  (true);
	constinit HeatParameters::Pair<bool> canLoseWhenInactives(true);

	// Code caves
	constinit ModContainers::DefaultCopyVaultMap<float> copTypeToBreakerChange(0.f); // seconds





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	address GetLocalPlayer()
	{
		const bool playerActive = (*reinterpret_cast<volatile size_t*>(0x92D884) > 0);
		return (playerActive) ? **reinterpret_cast<volatile address* volatile*>(0x92D87C) : 0x0;
	}



	void __stdcall ChargeSpeedbreaker(const float amount)
	{
		if (amount == 0.f) return;

		const address localPlayer = GetLocalPlayer();
		if (not localPlayer) return; // should never happen

		const bool isBreakerActive = *reinterpret_cast<volatile bool*>(localPlayer + 0x34);

		if (amount > 0.f)
		{
			if (isBreakerActive       and (not canGainWhenActives  .current)) return;
			if ((not isBreakerActive) and (not canGainWhenInactives.current)) return;
		}
		else
		{
			if (isBreakerActive       and (not canLoseWhenActives  .current)) return;
			if ((not isBreakerActive) and (not canLoseWhenInactives.current)) return;
		}

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<1>("[GBR] Speedbreaker change:", amount);

		const auto  ChargeGameBreaker = reinterpret_cast<void (__thiscall*)(address, float)>(0x6F8F60);
		const float timeToRatio       = **reinterpret_cast<volatile float* volatile*>(0x6EDDC3);

		ChargeGameBreaker(localPlayer, timeToRatio * amount);
	}



	bool IsPlayerInPursuit()
	{
		const address playerAIVehicle = Globals::GetAIVehicle(Globals::GetPlayerVehicle());
		return (playerAIVehicle and *reinterpret_cast<volatile address*>(playerAIVehicle + 0x70));
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

			fld dword ptr copWreckBreakerChanges.current
			fmul dword ptr Globals::floatScale

			push dword ptr [esi + 0xF8] // copType
			mov ecx, offset copTypeToBreakerChange
			call ModContainers::DefaultCopyVaultMap<float>::GetValue
			fmul dword ptr Globals::floatScale

			faddp st(1), st(0)

			push eax
			fstp dword ptr [esp] // amount
			call ChargeSpeedbreaker

			conclusion:
			// Execute original code and resume
			fld dword ptr [esi + 0xEC]

			jmp dword ptr wreckChangeExit
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

			cmp byte ptr driftRechargeEnableds.current, 0x1
			je conclusion // drift recharging unrestricted

			call IsPlayerInPursuit
			test al, al

			conclusion:
			jmp dword ptr driftRechargeExit
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

			cmp byte ptr passiveRechargeEnableds.current, 0x1
			je conclusion // passive recharging unrestricted

			call IsPlayerInPursuit
			test al, al

			conclusion:
			jmp dword ptr passiveRechargeExit
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseSpeedbreakerChanges(const HeatParameters::Parser& parser)
	{
		std::vector<const char*> copVehicles; // for game compatibility
		std::vector<float>       changes;

		parser.ParseUser<const char*, float>("Wrecking:Vehicles", copVehicles, {changes});

		return copTypeToBreakerChange.FillFromVectors
		(
			"Vehicle-to-change",
			Globals::GetVaultKey(HeatParameters::configDefaultKey),
			ModContainers::FillSetup(copVehicles, Globals::GetVaultKey, Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(changes)
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [GBR] GameBreaker");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Speedbreaker.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Speedbreaker:Mechanics", passiveRechargeEnableds, driftRechargeEnableds);
		HeatParameters::Parse(parser, "Speedbreaker:Wrecking",  copWreckBreakerChanges);

		HeatParameters::Parse(parser, "Wrecking:Gains",  canGainWhenActives, canGainWhenInactives);
		HeatParameters::Parse(parser, "Wrecking:Losses", canLoseWhenActives, canLoseWhenInactives);

		// Speedbreaker changes
		ParseSpeedbreakerChanges(parser);

		// Code changes
		MemoryTools::MakeRangeJMP<wreckChangeEntrance,     wreckChangeExit>    (WreckChange);
		MemoryTools::MakeRangeJMP<driftRechargeEntrance,   driftRechargeExit>  (DriftRecharge);
		MemoryTools::MakeRangeJMP<passiveRechargeEntrance, passiveRechargeExit>(PassiveRecharge);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [GBR] GameBreaker");

		passiveRechargeEnableds.Log("passiveRechargeEnabled  ");
		driftRechargeEnableds  .Log("driftRechargeEnabled    ");

		copWreckBreakerChanges.Log("copWreckBreakerChange   ");

		canGainWhenActives  .Log("canGainWhenActive       ");
		canGainWhenInactives.Log("canGainWhenInactive     ");

		canLoseWhenActives  .Log("canLoseWhenActive       ");
		canLoseWhenInactives.Log("canLoseWhenInactive     ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		passiveRechargeEnableds.SetToHeat(isRacing, heatLevel);
		driftRechargeEnableds  .SetToHeat(isRacing, heatLevel);

		copWreckBreakerChanges.SetToHeat(isRacing, heatLevel);

		canGainWhenActives  .SetToHeat(isRacing, heatLevel);
		canGainWhenInactives.SetToHeat(isRacing, heatLevel);

		canLoseWhenActives  .SetToHeat(isRacing, heatLevel);
		canLoseWhenInactives.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}