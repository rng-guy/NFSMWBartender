#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "ParameterSets.h"
#include "HeatParameters.h"



namespace NitrousCharge
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Value<bool> passiveRechargeEnabled(true);

	// Parameter sets
	RELEASE_CONSTINIT ParameterSets::CopInteractions nitrousInteractions; // seconds

	// Code caves
	float pendingCollisionNitrousChange = 0.f;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ChargeNitrous
	(
		const address perpVehicle, 
		const float   seconds
	) {
		const address perpAIVehicle = Globals::GetAIVehicleOfPerpVehicle(perpVehicle);
		const address engineRacer   = AsReference<address>(perpAIVehicle + 0x90);

		const address nitrousProperties = AsReference<address>(engineRacer       + 0xD0);
		const float   nitrousCapacity   = AsReference<float>  (nitrousProperties + 0x10);
		if (nitrousCapacity <= 0.f) return; // effectively has no nitrous

		if constexpr (Globals::loggingEnabled)
		{
			const address pursuit = Globals::GetPursuitOfPerpVehicle(perpVehicle);
			Globals::logger.Log(pursuit, "[NOS] Nitrous change:", seconds);
		}

		const auto ChargeNOS = AsFunction<void __thiscall (address, float)>(0x6A0470);
		ChargeNOS(engineRacer, Globals::floatScale * (seconds / nitrousCapacity));
	}



	bool __fastcall MayRechargePassively(const address engineRacer)
	{
		if (passiveRechargeEnabled.current) return true;

		const address vehicle = AsReference<address>(engineRacer - 0xC);

		const int driverClass = AsReference<int>(vehicle + 0x94);
		if ((driverClass != 0) and (driverClass != 3)) return true; // not racer

		const address racerAIVehicle = Globals::GetAIVehicleOfVehicle(vehicle);
		const address pursuit        = AsReference<address>(racerAIVehicle + 0x70);

		return (not pursuit);
	}



	void ProcessTaggedCop(const address copVehicle) 
	{
		pendingCollisionNitrousChange += nitrousInteractions.GetTaggingChange(copVehicle);
	}



	void ProcessAssaultedCop
	(
		const address copVehicle, 
		const byte    numCopAssaulted
	) {
		pendingCollisionNitrousChange += nitrousInteractions.GetAssaultChange(copVehicle, numCopAssaulted);
	}



	void ProcessFinishedCollision(const address perpVehicle)
	{
		if (pendingCollisionNitrousChange == 0.f) return;

		if (const address pursuit = Globals::GetPursuitOfPerpVehicle(perpVehicle))
			ChargeNitrous(perpVehicle, pendingCollisionNitrousChange);

		pendingCollisionNitrousChange = 0.f;
	}



	void ProcessDestroyedCop
	(
		const address pursuit,
		const address copVehicle
	) {
		const address perpVehicle = Globals::GetPerpVehicleOfPursuit(pursuit);
		if (not perpVehicle) return; // should never happen

		const float nitrousChange = nitrousInteractions.GetWreckingChange(copVehicle);

		if (nitrousChange != 0.f)
			ChargeNitrous(perpVehicle, nitrousChange);
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address passiveRechargeEntrance = 0x6929A4;
	constexpr address passiveRechargeExit     = 0x6929A9;

	// Toggles passive nitrous recharging
	__declspec(naked) void PassiveRecharge()
	{
		__asm
		{
			fnstsw ax
			test ah, 0x1
			jne conclusion // below speed threshold

			lea ecx, dword ptr [esi + 0x54]
			call MayRechargePassively // ecx: engineRacer
			cmp al, 1

			conclusion:
			jmp dword ptr [passiveRechargeExit]
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [NOS] RacerNitrious");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Nitrous.ini")) return false;

		// Heat parameters
		HeatParameters::Parse(parser, "Nitrous:Time", passiveRechargeEnabled);

		// Parameter sets
		nitrousInteractions.Parse(parser, "Nitrous");

		// Code changes
		MemoryTools::MakeRangeJMP<passiveRechargeEntrance, passiveRechargeExit>(PassiveRecharge);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		Globals::logger.Log("    HEAT [NOS] GameBreaker");

		passiveRechargeEnabled.Log("passiveRechargeEnabled  ");

		nitrousInteractions.Log
		(
			"copTagNitrousChange     ",
			"nitrousChangePerAssault ",
			"maxNumAssaultsPerCop    ",
			"copWreckNitrousChange   "
		);
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		passiveRechargeEnabled.SetToHeatState(isRacing, heatLevel);

		nitrousInteractions.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
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