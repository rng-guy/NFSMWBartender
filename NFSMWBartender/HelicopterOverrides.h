#pragma once

#include <string>
#include <algorithm>

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
	HeatParameters::Pair<float>       rammingCooldowns  (8.f);       // seconds

	HeatParameters::Pair<bool>  helicopterEnableds(false);
	HeatParameters::Pair<float> minSpawnDelays    (1.f);   // seconds
	HeatParameters::Pair<float> maxSpawnDelays    (1.f);
	HeatParameters::Pair<float> minDespawnDelays  (1.f);
	HeatParameters::Pair<float> maxDespawnDelays  (1.f);
	HeatParameters::Pair<float> minRespawnDelays  (1.f);
	HeatParameters::Pair<float> maxRespawnDelays  (1.f);

	HeatParameters::Pair<bool>  rejoiningEnableds(false);
	HeatParameters::Pair<float> minRejoinDelays  (1.f);   // seconds
	HeatParameters::Pair<float> maxRejoinDelays  (1.f);

	// Conversions
	float maxFuelTime = std::max<float>(maxDespawnDelays.current, maxRejoinDelays.current);

	// Code caves
	bool skipBailoutSpeech = false;





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::PursuitReaction
	{
		using IntervalTimer = PursuitFeatures::IntervalTimer;



	private:
		
		bool  isPlayerPursuit   = false;
		bool  hasSpawnedBefore  = false;
		float fuelAtLastDespawn = 0.f;

		int& numHelicoptersDeployed = *((int*)(this->pursuit + 0x150));

		IntervalTimer firstSpawnTimer = IntervalTimer(helicopterEnableds, minSpawnDelays,   maxSpawnDelays);
		IntervalTimer respawnTimer    = IntervalTimer(helicopterEnableds, minRespawnDelays, maxRespawnDelays);
		IntervalTimer rejoinTimer     = IntervalTimer(rejoiningEnableds,  minRejoinDelays,  maxRejoinDelays);

		IntervalTimer* spawnTimer = &(this->firstSpawnTimer);


		void VerifyPursuit()
		{
			static const auto IsPlayerPursuit = (bool (__thiscall*)(address))0x40AD80;

			this->isPlayerPursuit = IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Confirmed" : "Not", "player pursuit");
		}


		void MakeSpawnAttempt() const
		{
			if (not this->spawnTimer)               return;
			if (not this->isPlayerPursuit)          return;
			if (not this->spawnTimer->HasExpired()) return;

			static const address& copManager      = *((address*)0x989098);
			static const auto     SpawnHelicopter = (bool (__thiscall*)(address, address))0x4269A0;

			if (copManager)
			{ 
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Requesting helicopter");

				SpawnHelicopter(copManager, this->pursuit);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid AICopManager pointer");
		}


		static float* GetHelicopterFuel()
		{ 
			static const address& helicopterObject = *((address*)0x90D61C);
			return (helicopterObject) ? (float*)(helicopterObject + 0x7D8) : nullptr;
		}


		void SetHelicopterFuel() 
		{
			if (this->spawnTimer) return;

			float* const fuel = this->GetHelicopterFuel();

			if (fuel)
			{ 
				if (this->fuelAtLastDespawn > 0.f)
				{
					*fuel = this->fuelAtLastDespawn - this->rejoinTimer.GetLength();

					--(this->numHelicoptersDeployed);
					this->fuelAtLastDespawn = 0.f;
				}
				else *fuel = Globals::prng.GenerateNumber<float>(minDespawnDelays.current, maxDespawnDelays.current);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Despawning in", *fuel);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid fuel pointer");
		}


		void UpdateSpawnTimer(const IntervalTimer::Setting setting)
		{
			if (not this->spawnTimer) return;

			this->spawnTimer->Update(setting);

			if (this->fuelAtLastDespawn > 0.f)
			{
				if (setting == IntervalTimer::Setting::ALL)
					this->respawnTimer.Update(IntervalTimer::Setting::START);

				if ((not this->rejoinTimer.IsEnabled()) or (this->rejoinTimer.GetLength() >= this->fuelAtLastDespawn))
				{
					this->respawnTimer.Update(IntervalTimer::Setting::LENGTH);

					this->spawnTimer        = &(this->respawnTimer);
					this->fuelAtLastDespawn = 0.f;

					if constexpr (Globals::loggingEnabled)
					{
						if (not this->rejoinTimer.IsEnabled())
							Globals::logger.Log(this->pursuit, "[HEL] Rejoining not enabled");

						else
							Globals::logger.Log(this->pursuit, "[HEL] Insufficient fuel to rejoin");
					}
				}
			}
			
			if constexpr (Globals::loggingEnabled)
			{
				if (this->spawnTimer->IsEnabled())
				{
					if (this->fuelAtLastDespawn > 0.f)
					{
						Globals::logger.Log
						(
							this->pursuit, 
							"[HEL] Rejoining in", 
							this->spawnTimer->TimeLeft(), 
							"for", 
							this->fuelAtLastDespawn - this->rejoinTimer.GetLength()
						);
					}
					else
					{
						Globals::logger.Log
						(
							this->pursuit, 
							"[HEL]", 
							(this->hasSpawnedBefore) ? "Respawning" : "Spawning", 
							"in", 
							this->spawnTimer->TimeLeft()
						);
					}
				}
			}
		}



	public:

		explicit HelicopterManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			this->UpdateSpawnTimer(IntervalTimer::Setting::ALL);
		}


		void UpdateOnGameplay() override 
		{
			this->MakeSpawnAttempt();
		}


		void UpdateOnHeatChange() override 
		{
			this->UpdateSpawnTimer(IntervalTimer::Setting::LENGTH);
		}


		void UpdateOncePerPursuit() override
		{
			this->VerifyPursuit();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel != CopLabel::HELICOPTER) return;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL] Helicopter now active");

			this->spawnTimer       = nullptr;
			this->hasSpawnedBefore = true;
			this->SetHelicopterFuel();

			skipBailoutSpeech = false;
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel != CopLabel::HELICOPTER) return;

			const bool         isWrecked = PursuitFeatures::IsVehicleDestroyed(copVehicle);
			const float* const fuel      = this->GetHelicopterFuel();

			if (isWrecked or (not fuel) or (not (*fuel > 0.f)))
			{
				this->spawnTimer        = &(this->respawnTimer);
				this->fuelAtLastDespawn = 0.f;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Helicopter", (isWrecked) ? "wrecked" : "out of fuel");
			}
			else 
			{
				this->spawnTimer        = &(this->rejoinTimer);
				this->fuelAtLastDespawn = *fuel;
				skipBailoutSpeech       = true;

				if constexpr (Globals::loggingEnabled)
				{
					Globals::logger.Log(this->pursuit, "[HEL] Helicopter lost");
					Globals::logger.Log(this->pursuit, "[HEL] Fuel remaining:", this->fuelAtLastDespawn);
				}
			}

			this->UpdateSpawnTimer(IntervalTimer::Setting::ALL);
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address bailoutEntrance = 0x717C00;
	constexpr address bailoutExit     = 0x717C06;

	__declspec(naked) void Bailout()
	{
		__asm
		{
			cmp byte ptr skipBailoutSpeech, 0x1
			jne conclusion

			retn

			conclusion:
			// Execute original code and resume
			sub esp, 0x8
			push esi
			mov esi, ecx

			jmp dword ptr bailoutExit
		}
	}



	constexpr address fuelTimeEntrance = 0x42AD9B;
	constexpr address fuelTimeExit     = 0x42ADA1;

	__declspec(naked) void FuelTime()
	{
		__asm
		{
			mov ecx, dword ptr maxFuelTime

			// Execute original code and resume
			mov dword ptr [esi + 0x34], ecx

			jmp dword ptr fuelTimeExit
		}
	}



	constexpr address rammingCooldownEntrance = 0x4128B2;
	constexpr address rammingCooldownExit     = 0x4128B9;

	__declspec(naked) void RammingCooldown()
	{
		__asm
		{
			mov ecx, dword ptr rammingCooldowns.current

			// Execute original code and resume
			mov dword ptr [esi + 0x64], ecx

			jmp dword ptr rammingCooldownExit
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
			"Helicopter:Spawning", 
			helicopterEnableds,
			{minSpawnDelays,   1.f},
			{maxSpawnDelays,   1.f},
			{minDespawnDelays, 1.f},
			{maxDespawnDelays, 1.f},
			{minRespawnDelays, 1.f},
			{maxRespawnDelays, 1.f}
		);

		if (not helicopterEnableds.AnyNonzero()) return false;

		HeatParameters::CheckIntervals<float>(minSpawnDelays,   maxSpawnDelays);
		HeatParameters::CheckIntervals<float>(minDespawnDelays, maxDespawnDelays);
		HeatParameters::CheckIntervals<float>(minRespawnDelays, maxRespawnDelays);

		HeatParameters::Parse<std::string>(parser, "Helicopter:Vehicle", {helicopterVehicles});
		HeatParameters::Parse<float>      (parser, "Helicopter:Ramming", {rammingCooldowns, 1.f});

		HeatParameters::ParseOptional<float, float>
		(
			parser, 
			"Helicopter:Rejoining", 
			rejoiningEnableds, 
			{minRejoinDelays, 1.f}, 
			{maxRejoinDelays, 1.f}
		);

		HeatParameters::CheckIntervals<float>(minRejoinDelays,  maxRejoinDelays);

		// Code caves
		MemoryEditor::DigCodeCave(Bailout,         bailoutEntrance,         bailoutExit);
		MemoryEditor::DigCodeCave(FuelTime,        fuelTimeEntrance,        fuelTimeExit);
		MemoryEditor::DigCodeCave(RammingCooldown, rammingCooldownEntrance, rammingCooldownExit);

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
		helicopterVehicles.Validate(Globals::VehicleClass::CHOPPER, "Helicopters");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		helicopterVehicles.SetToHeat(isRacing, heatLevel);
		rammingCooldowns  .SetToHeat(isRacing, heatLevel);

		helicopterEnableds.SetToHeat(isRacing, heatLevel);
		minSpawnDelays    .SetToHeat(isRacing, heatLevel);
		maxSpawnDelays    .SetToHeat(isRacing, heatLevel);
		minDespawnDelays  .SetToHeat(isRacing, heatLevel);
		maxDespawnDelays  .SetToHeat(isRacing, heatLevel);
		minRespawnDelays  .SetToHeat(isRacing, heatLevel);
		maxRespawnDelays  .SetToHeat(isRacing, heatLevel);

		rejoiningEnableds.SetToHeat(isRacing, heatLevel);
		minRejoinDelays  .SetToHeat(isRacing, heatLevel);
		maxRejoinDelays  .SetToHeat(isRacing, heatLevel);

		maxFuelTime = std::max<float>(maxDespawnDelays.current, maxRejoinDelays.current);

		if constexpr (Globals::loggingEnabled)
		{
			if (helicopterEnableds.current or rejoiningEnableds.current)
			{
				Globals::logger.Log("    HEAT [HEL] HelicopterOverrides");
				Globals::logger.LogLongIndent("helicopterVehicle       ", helicopterVehicles.current);
				Globals::logger.LogLongIndent("rammingCooldown         ", rammingCooldowns.current);
			}

			if (helicopterEnableds.current)
			{
				Globals::logger.LogLongIndent("spawnDelay              ", minSpawnDelays.current,   "to", maxSpawnDelays.current);
				Globals::logger.LogLongIndent("despawnDelay            ", minDespawnDelays.current, "to", maxDespawnDelays.current);
				Globals::logger.LogLongIndent("respawnDelay            ", minRespawnDelays.current, "to", maxRespawnDelays.current);
			}

			if (rejoiningEnableds.current)
				Globals::logger.LogLongIndent("rejoinDelay             ", minRejoinDelays.current, "to", maxRejoinDelays.current);
		}
	}
}