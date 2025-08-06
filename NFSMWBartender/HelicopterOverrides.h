#pragma once

#include <Windows.h>

#include "Globals.h"
#include "PursuitFeatures.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace HelicopterOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	bool  helicopterEnabled = false;
	float minSpawnDelay     = 0.f;
	float maxSpawnDelay     = 0.f;
	float minRespawnDelay   = 0.f;
	float maxRespawnDelay   = 0.f;
	float minDespawnDelay   = 0.f;
	float maxDespawnDelay   = 0.f;

	// Heat levels
	Globals::HeatParametersPair<bool>  helicopterEnableds;
	Globals::HeatParametersPair<float> minSpawnDelays;
	Globals::HeatParametersPair<float> maxSpawnDelays;
	Globals::HeatParametersPair<float> minDespawnDelays;
	Globals::HeatParametersPair<float> maxDespawnDelays;
	Globals::HeatParametersPair<float> minRespawnDelays;
	Globals::HeatParametersPair<float> maxRespawnDelays;





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::CopVehicleReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:
		
		const address pursuit;

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
				Globals::Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Confirmed" : "Not", "player pursuit");
		}


		void MakeSpawnAttempt() const
		{
			static char (__thiscall* const SpawnHelicopter)(address, address) = (char (__thiscall*)(address, address))0x4269A0;

			if (not helicopterEnabled)        return;
			if (not this->isPlayerPursuit)    return;
			if (not this->IsHeatLevelKnown()) return;

			if (this->helicopterActive)                             return;
			if (*(this->simulationTime) < this->nextSpawnTimestamp) return;

			static const address* const copManager = (address*)0x989098;

			if (*copManager) 
			{ 
				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[HEL] Requesting helicopter");

				SpawnHelicopter(*copManager, this->pursuit);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [HEL] Invalid AICopManager pointer");
		}


		void SetHelicopterFuel(const float fuel) const
		{
			static const address* const helicopterObject = (address*)0x90D61C;

			if (*helicopterObject)
			{ 
				*((float*)(*helicopterObject + 0x7D8)) = fuel;

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[HEL] Despawning in", fuel);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [HEL] Invalid helicopter pointer");
		}


		void UpdateNextSpawnTimestamp()
		{
			if (not helicopterEnabled)        return;
			if (not this->IsHeatLevelKnown()) return;
			if (this->helicopterActive)       return;

			this->nextSpawnTimestamp = this->statusChangeTimestamp;

			if (this->hasSpawnedBefore)
			{
				this->nextSpawnTimestamp += Globals::prng.Generate<float>(minRespawnDelay, maxRespawnDelay);

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[HEL] Respawning in", this->nextSpawnTimestamp - *(this->simulationTime));
			}
			else
			{
				this->nextSpawnTimestamp += Globals::prng.Generate<float>(minSpawnDelay, maxSpawnDelay);

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[HEL] Spawning in", this->nextSpawnTimestamp - *(this->simulationTime));
			}
		}


		void ChangeStatus()
		{
			this->helicopterActive      = (not this->helicopterActive);
			this->statusChangeTimestamp = *(this->simulationTime);

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[HEL] Helicopter now", (this->helicopterActive) ? "active" : "inactive");

			if (this->helicopterActive)
			{
				this->hasSpawnedBefore = true;
				this->SetHelicopterFuel(Globals::prng.Generate<float>(minDespawnDelay, maxDespawnDelay));
			}
			else this->UpdateNextSpawnTimestamp();
		}



	public:

		HelicopterManager(const address pursuit) 
			: pursuit(pursuit) 
		{
			this->UpdateNextSpawnTimestamp();
		}


		void UpdateOnGameplay() override 
		{
			this->MakeSpawnAttempt();
		}


		void UpdateOnHeatChange()  override 
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
			const hash     copType,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::HELICOPTER) this->ChangeStatus();
		}


		void ProcessRemoval
		(
			const address  copVehicle,
			const address  copType,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel == CopLabel::HELICOPTER) this->ChangeStatus();
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address fuelTimeEntrance = 0x42AD9B;
	constexpr address fuelTimeExit     = 0x42ADA1;

	__declspec(naked) void FuelTime()
	{
		__asm
		{
			mov ecx, dword ptr maxDespawnDelay

			// Execute original code and resume
			mov dword ptr [esi + 0x34], ecx

			jmp dword ptr fuelTimeExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		MemoryEditor::Write<byte>(0xEB, 0x42BB2D); // prevents spawns in Cooldown mode through VltEd

		if (not parser.LoadFile(Globals::configPathAdvanced + "Helicopter.ini")) return false;

		// Heat parameters
		for (const bool forRaces : {false, true})
		{
			helicopterEnableds(forRaces) = parser.ParseParameterTable<float, float, float, float, float, float>
			(
				"Helicopter:Timers",
				(forRaces) ? Globals::configFormatRace : Globals::configFormatRoam,
				Globals::FormatParameter<float>(minSpawnDelays  (forRaces), {}, 0.f),
				Globals::FormatParameter<float>(maxSpawnDelays  (forRaces), {}, 0.f),
				Globals::FormatParameter<float>(minDespawnDelays(forRaces), {}, 0.f),
				Globals::FormatParameter<float>(maxDespawnDelays(forRaces), {}, 0.f),
				Globals::FormatParameter<float>(minRespawnDelays(forRaces), {}, 0.f),
				Globals::FormatParameter<float>(maxRespawnDelays(forRaces), {}, 0.f)
			);
		}

		Globals::CheckIntervalValues<float>(minSpawnDelays,   maxSpawnDelays);
		Globals::CheckIntervalValues<float>(minDespawnDelays, maxDespawnDelays);
		Globals::CheckIntervalValues<float>(minRespawnDelays, maxRespawnDelays);

		// Code caves
		MemoryEditor::DigCodeCave(FuelTime, fuelTimeEntrance, fuelTimeExit);

		return (featureEnabled = true);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		if (helicopterEnabled = helicopterEnableds(isRacing, heatLevel))
		{
			minSpawnDelay   = minSpawnDelays  (isRacing, heatLevel);
			maxSpawnDelay   = maxSpawnDelays  (isRacing, heatLevel);
			minDespawnDelay = minDespawnDelays(isRacing, heatLevel);
			maxDespawnDelay = maxDespawnDelays(isRacing, heatLevel);
			minRespawnDelay = minRespawnDelays(isRacing, heatLevel);
			maxRespawnDelay = maxRespawnDelays(isRacing, heatLevel);
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[HEL] HelicopterOverrides");
			Globals::LogLongIndent("Helicopter", (helicopterEnabled) ? "enabled" : "disabled");

			if (helicopterEnabled)
			{
				Globals::LogLongIndent("minSpawnDelay          :", minSpawnDelay);
				Globals::LogLongIndent("maxSpawnDelay          :", maxSpawnDelay);
				Globals::LogLongIndent("minDespawnDelay        :", minDespawnDelay);
				Globals::LogLongIndent("maxDespawnDelay        :", maxDespawnDelay);
				Globals::LogLongIndent("minRespawnDelay        :", minRespawnDelay);
				Globals::LogLongIndent("maxRespawnDelay        :", maxRespawnDelay);
			}
		}
	}
}