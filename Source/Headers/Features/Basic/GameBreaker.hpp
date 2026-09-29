#pragma once

#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ParameterSets.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace GameBreaker
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[GBR]";
	constexpr Globals::LogLiteral logName = "GameBreaker";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(bool, passiveRechargeEnabled, true);
	constinit HEAT_PARAMETER_VALUE(bool, driftRechargeEnabled,   true);

	// Cop interactions
	RELEASE_CONSTINIT ParameterSets::CopInteractions breakerInteractions; // seconds
	constinit         ParameterSets::ChangeFilter    breakerChangeFilter;

	// Assembly detours
	float pendingCollisionBreakerChange = 0.f;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ChargeSpeedbreakerOfTarget
	(
		const address pursuit, 
		const float   seconds
	) {
		const address localPlayer = Globals::Pursuit::GetLocalPlayer(pursuit);
		if (not localPlayer) return; // not player pursuit

		const bool isBreakerEngaged = AsReference<bool>(localPlayer + 0x34);
		if (not breakerChangeFilter.IsAllowedChange(isBreakerEngaged, seconds)) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogFull(pursuit, logTag, "Speedbreaker change:", seconds);

		const auto  ChargeGameBreaker = AsFunction  <void __thiscall (address, float)>(0x6F8F60);
		const float timeToRatio       = *AsReference<const float* const>              (0x6EDDC3);

		ChargeGameBreaker(localPlayer, Globals::floatScale * (timeToRatio * seconds));
	}



	[[nodiscard]] bool __fastcall IsInPursuit(const address localPlayer)
	{
		ASSERT_CONDITION_THEN_IF_FALSE(localPlayer, return false);

		for (const address pursuit : ModContainers::PursuitView())
		{
			if (localPlayer == Globals::Pursuit::GetLocalPlayer(pursuit)) return true;
		}

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

		if (const address pursuit = Globals::PerpVehicle::GetPursuit(perpVehicle))
			ChargeSpeedbreakerOfTarget(pursuit, pendingCollisionBreakerChange);

		pendingCollisionBreakerChange = 0.f;
	}



	void ProcessDestroyedCop
	(
		const address pursuit,
		const address copVehicle
	) {
		const float breakerChange = breakerInteractions.GetWreckingChange(copVehicle);

		if (breakerChange != 0.f)
			ChargeSpeedbreakerOfTarget(pursuit, breakerChange);
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Toggles drift-based Speedbreaker recharging for player vehicles
	ASSEMBLY_DETOUR(DriftRecharge, /* begin = */ 0x6A99BC, /* end = */ 0x6A99C1)
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
			EXIT_ASSEMBLY_DETOUR(DriftRecharge)
		}
	}



	// Toggles passive Speedbreaker recharging for player vehicles
	ASSEMBLY_DETOUR(PassiveRecharge, 0x6EDDDE, 0x6EDDE3)
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
			EXIT_ASSEMBLY_DETOUR(PassiveRecharge)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	void ExtractCopInteractions(const ConfigParser::Parser& parser)
	{
		constexpr std::string_view featureTag = "Speedbreaker";

		breakerInteractions.Extract(parser, featureTag);
		breakerChangeFilter.Extract(parser, featureTag);
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileSpeedbreaker)) return false;

		// Heat parameters
		HeatParameters::Extract(parser, "Speedbreaker:Mechanics", passiveRechargeEnabled, driftRechargeEnabled);

		// Cop interactions
		ExtractCopInteractions(parser);

		// Code modifications
		PATCH_ASSEMBLY_DETOUR(DriftRecharge);
		PATCH_ASSEMBLY_DETOUR(PassiveRecharge);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		// Heat parameters
		passiveRechargeEnabled.SetToHeatState(state);
		driftRechargeEnabled  .SetToHeatState(state);

		// Cop interactions
		breakerInteractions.SetToHeatState(state);
		breakerChangeFilter.SetToHeatState(state);
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