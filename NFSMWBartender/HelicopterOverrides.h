#pragma once

#include "Globals.h"
#include "PursuitFeatures.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace HelicopterOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<std::string> helicopterVehicles("copheli");

	HeatParameters::Pair<bool>  helicopterEnableds(false);
	HeatParameters::Pair<float> minSpawnDelays    (1.f);   // seconds
	HeatParameters::Pair<float> maxSpawnDelays    (1.f);
	HeatParameters::Pair<float> minDespawnDelays  (1.f);
	HeatParameters::Pair<float> maxDespawnDelays  (1.f);
	HeatParameters::Pair<float> minRespawnDelays  (1.f);
	HeatParameters::Pair<float> maxRespawnDelays  (1.f);





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::PursuitReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:
		
		bool isPlayerPursuit  = false;
		bool hasSpawnedBefore = false;

		bool  helicopterActive      = false;
		float statusChangeTimestamp = *(this->simulationTime);
		float nextSpawnTimestamp    = 0.f;


		void VerifyPursuit()
		{
			static bool (__thiscall* const IsPlayerPursuit)(address) = (bool (__thiscall*)(address))0x40AD80;

			this->isPlayerPursuit = IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Confirmed" : "Not", "player pursuit");
		}


		void MakeSpawnAttempt() const
		{
			static bool (__thiscall* const SpawnHelicopter)(address, address) = (bool (__thiscall*)(address, address))0x4269A0;

			if (not helicopterEnableds.current) return;
			if (not this->isPlayerPursuit)      return;
			if (not this->IsHeatLevelKnown())   return;

			if (this->helicopterActive)                             return;
			if (*(this->simulationTime) < this->nextSpawnTimestamp) return;

			static const address* const copManager = (address*)0x989098;

			if (*copManager) 
			{ 
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Requesting helicopter");

				SpawnHelicopter(*copManager, this->pursuit);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid AICopManager pointer");
		}


		void SetHelicopterFuel(const float fuel) const
		{
			static const address* const helicopterObject = (address*)0x90D61C;

			if (*helicopterObject)
			{ 
				*((float*)(*helicopterObject + 0x7D8)) = fuel;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Despawning in", fuel);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid helicopter pointer");
		}


		void UpdateNextSpawnTimestamp()
		{
			if (not helicopterEnableds.current) return;
			if (not this->IsHeatLevelKnown())   return;
			if (this->helicopterActive)         return;

			this->nextSpawnTimestamp = this->statusChangeTimestamp;

			if (this->hasSpawnedBefore)
			{
				this->nextSpawnTimestamp += Globals::prng.Generate<float>(minRespawnDelays.current, maxRespawnDelays.current);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Respawning in", this->nextSpawnTimestamp - *(this->simulationTime));
			}
			else
			{
				this->nextSpawnTimestamp += Globals::prng.Generate<float>(minSpawnDelays.current, maxSpawnDelays.current);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Spawning in", this->nextSpawnTimestamp - *(this->simulationTime));
			}
		}


		void ChangeStatus()
		{
			this->statusChangeTimestamp = *(this->simulationTime);
			this->helicopterActive      = (not this->helicopterActive);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL] Helicopter now", (this->helicopterActive) ? "active" : "inactive");

			if (this->helicopterActive)
			{
				this->hasSpawnedBefore = true;
				this->SetHelicopterFuel(Globals::prng.Generate<float>(minDespawnDelays.current, maxDespawnDelays.current));
			}
			else this->UpdateNextSpawnTimestamp();
		}



	public:

		explicit HelicopterManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			this->UpdateNextSpawnTimestamp();
		}


		void UpdateOnGameplay() override 
		{
			this->MakeSpawnAttempt();
		}


		void UpdateOnHeatChange() override 
		{
			this->UpdateNextSpawnTimestamp();
		}


		void UpdateOnceInPursuit() override 
		{
			this->VerifyPursuit();
		}


		void ProcessAddition
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::HELICOPTER) 
				this->ChangeStatus();
		}


		void ProcessRemoval
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::HELICOPTER) 
				this->ChangeStatus();
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address fuelTimeEntrance = 0x42AD9B;
	constexpr address fuelTimeExit     = 0x42ADA1;

	__declspec(naked) void FuelTime()
	{
		__asm
		{
			mov ecx, dword ptr maxDespawnDelays.current

			// Execute original code and resume
			mov dword ptr [esi + 0x34], ecx

			jmp dword ptr fuelTimeExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathAdvanced + "Helicopter.ini")) return false;

		// Heat parameters
		HeatParameters::ParseOptional<float, float, float, float, float, float>
		(
			parser, 
			"Helicopter:Timers", 
			helicopterEnableds,
			{minSpawnDelays,   1.f},
			{maxSpawnDelays,   1.f},
			{minDespawnDelays, 1.f},
			{maxDespawnDelays, 1.f},
			{minRespawnDelays, 1.f},
			{maxRespawnDelays, 1.f}
		);

		if (not HeatParameters::AreAny(helicopterEnableds)) return false;

		HeatParameters::CheckIntervals<float>(minSpawnDelays,   maxSpawnDelays);
		HeatParameters::CheckIntervals<float>(minDespawnDelays, maxDespawnDelays);
		HeatParameters::CheckIntervals<float>(minRespawnDelays, maxRespawnDelays);

		HeatParameters::Parse<std::string>(parser, "Helicopter:Vehicle", {helicopterVehicles});

		// Code caves
		MemoryEditor::DigCodeCave(FuelTime, fuelTimeEntrance, fuelTimeExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [HEL] HelicopterOverrides");

		// With logging disabled, the compiler optimises the string literal away
		HeatParameters::ReplaceInvalidVehicles("Helicopters", helicopterVehicles, true);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		helicopterEnableds.SetToHeat(isRacing, heatLevel);
		helicopterVehicles.SetToHeat(isRacing, heatLevel);

		minSpawnDelays  .SetToHeat(isRacing, heatLevel);
		maxSpawnDelays  .SetToHeat(isRacing, heatLevel);
		minDespawnDelays.SetToHeat(isRacing, heatLevel);
		maxDespawnDelays.SetToHeat(isRacing, heatLevel);
		minRespawnDelays.SetToHeat(isRacing, heatLevel);
		maxRespawnDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [HEL] HelicopterOverrides");
			Globals::logger.LogLongIndent("Helicopter", (helicopterEnableds.current) ? "enabled" : "disabled");

			if (helicopterEnableds.current)
			{
				Globals::logger.LogLongIndent("helicopterVehicle       ", helicopterVehicles.current);

				Globals::logger.LogLongIndent("minSpawnDelay           ", minSpawnDelays.current);
				Globals::logger.LogLongIndent("maxSpawnDelay           ", maxSpawnDelays.current);
				Globals::logger.LogLongIndent("minDespawnDelay         ", minDespawnDelays.current);
				Globals::logger.LogLongIndent("maxDespawnDelay         ", maxDespawnDelays.current);
				Globals::logger.LogLongIndent("minRespawnDelay         ", minRespawnDelays.current);
				Globals::logger.LogLongIndent("maxRespawnDelay         ", maxRespawnDelays.current);
			}
		}
	}
}