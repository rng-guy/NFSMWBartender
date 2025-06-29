#pragma once

#include <Windows.h>
#include <algorithm>
#include <array>

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

	// General Heat levels
	std::array<bool,  Globals::maxHeatLevel> helicoptersEnabled = {};
	std::array<float, Globals::maxHeatLevel> minSpawnDelays     = {};
	std::array<float, Globals::maxHeatLevel> maxSpawnDelays     = {};
	std::array<float, Globals::maxHeatLevel> minDespawnDelays   = {};
	std::array<float, Globals::maxHeatLevel> maxDespawnDelays   = {};
	std::array<float, Globals::maxHeatLevel> minRespawnDelays   = {};
	std::array<float, Globals::maxHeatLevel> maxRespawnDelays   = {};





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::CopVehicleReaction
	{
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
				*(float*)(*helicopterObject + 0x7D8) = fuel; 

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
				this->nextSpawnTimestamp += this->prng.GetFloat(maxRespawnDelay, minRespawnDelay);

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[HEL] Respawning in", this->nextSpawnTimestamp - *(this->simulationTime));
			}
			else
			{
				this->nextSpawnTimestamp += this->prng.GetFloat(maxSpawnDelay, minSpawnDelay);

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
				this->SetHelicopterFuel(this->prng.GetFloat(maxDespawnDelay, minDespawnDelay));
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
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[HEL] Helicopter now", (helicopterEnabled) ? "enabled" : "disabled");

			this->UpdateNextSpawnTimestamp();
		}


		void UpdateOnceInPursuit() override 
		{
			this->VerifyPursuit();
		}


		void ProcessAddition
		(
			const address                   copVehicle,
			const hash                      copType,
			const PursuitFeatures::CopLabel copLabel
		) 
			override
		{
			if (copLabel == PursuitFeatures::CopLabel::HELICOPTER) this->ChangeStatus();
		}


		void ProcessRemoval
		(
			const address                   copVehicle,
			const address                   copType,
			const PursuitFeatures::CopLabel copLabel
		) 
			override
		{
			if (copLabel == PursuitFeatures::CopLabel::HELICOPTER) this->ChangeStatus();
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	const address fuelTimeEntrance = 0x42AD9B;
	const address fuelTimeExit     = 0x42ADA1;

	__declspec(naked) void FuelTime()
	{
		__asm
		{
			mov ecx, maxDespawnDelay

			// Execute original code and resume
			mov [esi + 0x34], ecx

			jmp dword ptr fuelTimeExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		if (not parser.LoadFile(Globals::configAdvancedPath + "Helicopter.ini")) return;

		helicoptersEnabled = parser.ParseParameterTable
		(
			"Helicopter:Timers",
			Globals::configFormat,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minSpawnDelays,   {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxSpawnDelays,   {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minDespawnDelays, {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxDespawnDelays, {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minRespawnDelays, {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxRespawnDelays, {}, 0.f)
		);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			minSpawnDelays[heatLevel - 1]   = std::min(minSpawnDelays[heatLevel - 1],   maxSpawnDelays[heatLevel - 1]);
			minDespawnDelays[heatLevel - 1] = std::min(minDespawnDelays[heatLevel - 1], maxDespawnDelays[heatLevel - 1]);
			minRespawnDelays[heatLevel - 1] = std::min(minRespawnDelays[heatLevel - 1], maxRespawnDelays[heatLevel - 1]);
		}

		MemoryEditor::Write<BYTE>(0xEB, 0x42BB2D); // prevent spawns in Cooldown mode
		MemoryEditor::DigCodeCave(&FuelTime, fuelTimeEntrance, fuelTimeExit);

		featureEnabled = true;
	}



	void SetToHeat(size_t heatLevel)
	{
		if (not featureEnabled)                                          return;
		if (not (helicopterEnabled = helicoptersEnabled[heatLevel - 1])) return;

		minSpawnDelay   = minSpawnDelays[heatLevel - 1];
		maxSpawnDelay   = maxSpawnDelays[heatLevel - 1];
		minDespawnDelay = minDespawnDelays[heatLevel - 1];
		maxDespawnDelay = maxDespawnDelays[heatLevel - 1];
		minRespawnDelay = minRespawnDelays[heatLevel - 1];
		maxRespawnDelay = maxRespawnDelays[heatLevel - 1];
	}
}