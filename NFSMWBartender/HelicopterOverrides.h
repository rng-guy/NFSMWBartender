#pragma once

#include <string>
#include <limits>
#include <algorithm>

#include "Globals.h"
#include "PursuitFeatures.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"

// Check rejoining and fleeing without logging

namespace HelicopterOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<std::string> helicopterVehicles("copheli");
	HeatParameters::Pair<float>       rammingCooldowns  (8.f);       // seconds

	HeatParameters::OptionalInterval spawnDelays;
	HeatParameters::OptionalInterval respawnDelays;
	HeatParameters::OptionalInterval fuelLimits;
	HeatParameters::OptionalInterval rejoinDelays;

	// Conversions
	float bailoutThreshold = (rejoinDelays.isEnableds.current) ? std::min<float>(rejoinDelays.minValues.current, 8.f) : 8.f;

	// Code caves
	bool hasLimitedFuel    = false;
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

		IntervalTimer firstSpawnTimer = IntervalTimer(spawnDelays);
		IntervalTimer respawnTimer    = IntervalTimer(respawnDelays);
		IntervalTimer rejoinTimer     = IntervalTimer(rejoinDelays);

		IntervalTimer* spawnTimer = &(this->firstSpawnTimer);

		inline static address helicopterOwner = 0x0;


		void VerifyPursuit()
		{
			static const auto IsPlayerPursuit = (bool (__thiscall*)(address))0x40AD80;

			this->isPlayerPursuit = IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Confirmed" : "Not", "player pursuit");
		}


		bool IsOwner() const
		{
			return (this->helicopterOwner == this->pursuit);
		}


		void MakeSpawnAttempt() const
		{
			if (this->helicopterOwner and (not this->IsOwner())) return;

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


		bool IsRejoining() const
		{
			return (this->spawnTimer == &(this->rejoinTimer));
		}


		static float* GetHelicopterFuelPointer()
		{ 
			static const address& helicopter = *((address*)0x90D61C);
			return (helicopter) ? (float*)(helicopter + 0x7D8) : nullptr;
		}


		void SetHelicopterFuel(const float fuelTime) const
		{
			if (this->spawnTimer)    return;
			if (not this->IsOwner()) return;

			float* const fuel = this->GetHelicopterFuelPointer();

			if (fuel)
			{
				*fuel = fuelTime;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Fuel time:", fuelTime);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid fuel pointer");
		}


		void UpdateSpawnTimer(const IntervalTimer::Setting setting)
		{
			if (not this->spawnTimer) return;

			this->spawnTimer->Update(setting);

			if (this->IsRejoining())
			{
				if (setting == IntervalTimer::Setting::ALL)
					this->respawnTimer.Update(IntervalTimer::Setting::START);

				if ((not this->rejoinTimer.IsEnabled()) or (hasLimitedFuel and (this->rejoinTimer.GetLength() >= this->fuelAtLastDespawn)))
				{
					this->respawnTimer.Update(IntervalTimer::Setting::LENGTH);

					this->spawnTimer        = &(this->respawnTimer);
					this->helicopterOwner   = 0x0;
					this->fuelAtLastDespawn = 0.f;

					if constexpr (Globals::loggingEnabled)
					{
						if (PursuitFeatures::heatLevelKnown)
						{
							if (not this->rejoinTimer.IsEnabled())
								Globals::logger.Log(this->pursuit, "[HEL] Rejoining not enabled");

							else
								Globals::logger.Log(this->pursuit, "[HEL] Insufficient fuel to rejoin");
						}
					}
				}
			}
			
			if constexpr (Globals::loggingEnabled)
			{
				if (this->spawnTimer->IsEnabled())
				{
					if (this->IsRejoining())
					{
						if (hasLimitedFuel)
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
						else Globals::logger.Log(this->pursuit, "[HEL] Rejoining in", this->spawnTimer->TimeLeft());
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


		~HelicopterManager() override
		{
			if (this->IsOwner())
				this->helicopterOwner = 0x0;
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
			{
				Globals::logger.Log(this->pursuit, "[HEL] Helicopter now active");

				if (this->helicopterOwner and (not this->IsOwner()))
					Globals::logger.Log("WARNING: [HEL] Owner mismatch:", this->helicopterOwner, '/', this->pursuit);
			}

			const bool wasRejoining = this->IsRejoining();

			if (not this->helicopterOwner)
				hasLimitedFuel = fuelLimits.isEnableds.current;

			this->spawnTimer       = nullptr;
			this->helicopterOwner  = this->pursuit;
			this->hasSpawnedBefore = true;

			if (wasRejoining)
			{
				if (hasLimitedFuel)
					this->SetHelicopterFuel(this->fuelAtLastDespawn - this->rejoinTimer.GetLength());

				--(this->numHelicoptersDeployed);
				this->fuelAtLastDespawn = 0.f;
			}
			else if (hasLimitedFuel) 
				this->SetHelicopterFuel(fuelLimits.GetRandomValue());
			
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

			const bool         isWrecked = this->IsVehicleDestroyed(copVehicle);
			const float* const fuel      = this->GetHelicopterFuelPointer();

			if (isWrecked or (not fuel) or (not (*fuel > 0.f)))
			{
				this->spawnTimer        = &(this->respawnTimer);
				this->helicopterOwner   = 0x0;
				this->fuelAtLastDespawn = 0.f;

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Helicopter", (isWrecked) ? "wrecked" : "out of fuel");
			}
			else
			{
				this->spawnTimer        = &(this->rejoinTimer);
				this->fuelAtLastDespawn = *fuel;

				skipBailoutSpeech = true;

				if constexpr (Globals::loggingEnabled)
				{
					Globals::logger.Log(this->pursuit, "[HEL] Helicopter lost");

					if (hasLimitedFuel)
						Globals::logger.Log(this->pursuit, "[HEL] Fuel remaining:", this->fuelAtLastDespawn);
				}
			}

			this->UpdateSpawnTimer(IntervalTimer::Setting::ALL);
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address fuelUpdateEntrance = 0x423519;
	constexpr address fuelUpdateExit     = 0x423523;

	__declspec(naked) void FuelUpdate()
	{
		__asm
		{
			cmp byte ptr hasLimitedFuel, 0x1
			jne conclusion // unlimited fuel

			// Execute original code and resume
			fsub dword ptr [esp + 0x8]
			fst dword ptr [esi + 0x7D8]

			conclusion:
			jmp dword ptr fuelUpdateExit
		}
	}



	constexpr address defaultFuelEntrance = 0x42AD9B;
	constexpr address defaultFuelExit     = 0x42ADA1;

	__declspec(naked) void DefaultFuel()
	{
		static constexpr float fuelTime = std::numeric_limits<float>::max();

		__asm
		{
			mov ecx, dword ptr fuelTime
			mov dword ptr [esi + 0x34], ecx

			jmp dword ptr defaultFuelExit
		}
	}



	constexpr address earlyBailoutEntrance = 0x717C00;
	constexpr address earlyBailoutExit     = 0x717C06;

	__declspec(naked) void EarlyBailout()
	{
		__asm
		{
			cmp byte ptr skipBailoutSpeech, 0x1
			jne conclusion // speech enabled

			retn

			conclusion:
			// Execute original code and resume
			sub esp, 0x8
			push esi
			mov esi, ecx

			jmp dword ptr earlyBailoutExit
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
		if (not HeatParameters::Parse(parser, "Helicopter:Spawning", spawnDelays)) return false;

		HeatParameters::Parse(parser, "Helicopter:Respawning", respawnDelays);
		HeatParameters::Parse(parser, "Helicopter:Fuel",       fuelLimits);
		HeatParameters::Parse(parser, "Helicopter:Rejoining",  rejoinDelays);

		HeatParameters::Parse<std::string>(parser, "Helicopter:Vehicle", {helicopterVehicles});
		HeatParameters::Parse<float>      (parser, "Helicopter:Ramming", {rammingCooldowns, 1.f});

		// Code caves
		MemoryEditor::Write<float*>(&bailoutThreshold, {0x709F9F, 0x7078B0});

		MemoryEditor::DigCodeCave(FuelUpdate,      fuelUpdateEntrance,      fuelUpdateExit);
		MemoryEditor::DigCodeCave(DefaultFuel,     defaultFuelEntrance,     defaultFuelExit);
		MemoryEditor::DigCodeCave(EarlyBailout,    earlyBailoutEntrance,    earlyBailoutExit);
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
		helicopterVehicles.Validate("Helicopters", Globals::VehicleClass::CHOPPER);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		helicopterVehicles.SetToHeat(isRacing, heatLevel);
		rammingCooldowns  .SetToHeat(isRacing, heatLevel);

		spawnDelays  .SetToHeat(isRacing, heatLevel);
		respawnDelays.SetToHeat(isRacing, heatLevel);
		fuelLimits   .SetToHeat(isRacing, heatLevel);
		rejoinDelays .SetToHeat(isRacing, heatLevel);

		bailoutThreshold = (rejoinDelays.isEnableds.current) ? std::min<float>(rejoinDelays.minValues.current, 8.f) : 8.f;

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [HEL] HelicopterOverrides");
			Globals::logger.LogLongIndent("helicopterVehicle       ", helicopterVehicles.current);
			Globals::logger.LogLongIndent("rammingCooldown         ", rammingCooldowns.current);

			spawnDelays  .Log("spawnDelay              ");
			respawnDelays.Log("respawnDelay            ");
			fuelLimits   .Log("fuelLimit               ");
			rejoinDelays .Log("rejoinDelay             ");
		}
	}
}