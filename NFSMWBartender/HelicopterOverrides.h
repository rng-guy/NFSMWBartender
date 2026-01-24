#pragma once

#include <string>
#include <limits>
#include <algorithm>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace HelicopterOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::PointerPair<std::string> helicopterVehicles("copheli");

	HeatParameters::Interval<float>rammingCooldowns(8.f, 8.f, 1.f);  // seconds

	HeatParameters::OptionalInterval<float> firstSpawnDelays  (1.f); // seconds
	HeatParameters::OptionalInterval<float> fuelRespawnDelays (1.f); // seconds
	HeatParameters::OptionalInterval<float> wreckRespawnDelays(1.f); // seconds
	HeatParameters::OptionalInterval<float> lostRespawnDelays (1.f); // seconds

	HeatParameters::OptionalInterval<float> lostRejoinDelays  (1.f);       // seconds
	HeatParameters::Pair            <float> minRejoinFuelTimes(10.f, 1.f); // seconds

	HeatParameters::OptionalInterval<float> fuelTimes(1.f); // seconds

	// Code caves 
	bool hasLimitedFuel    = false;
	bool skipBailoutSpeech = false;

	float maxBailoutFuelTime = 8.f; // seconds





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::PursuitReaction
	{
	private:
		
		bool isPlayerPursuit = false;

		int& numHelicoptersDeployed = *reinterpret_cast<int*>(this->pursuit + 0x150);

		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST,
			REJOINING
		};

		Status helicopterStatus = Status::PENDING;

		PursuitFeatures::IntervalTimer spawnTimer;

		float fuelTimeOnRejoin  = 0.f;  // seconds
		float minRejoinFuelTime = 10.f; // seconds

		inline static address     helicopterOwner   = 0x0;
		inline static const char* helicopterVehicle = nullptr;

		inline static const address& helicopterObject = *reinterpret_cast<address*>(0x90D61C);


		void VerifyPursuit()
		{
			this->isPlayerPursuit = Globals::IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Is" : "Not", "player pursuit");
		}


		bool IsOwner() const
		{
			return (this->helicopterOwner == this->pursuit);
		}


		void TakeOwnership(const address copVehicle) const
		{
			if constexpr (Globals::loggingEnabled)
			{
				if (this->helicopterOwner and (not this->IsOwner()))
					Globals::logger.Log("WARNING: [HEL] Owner mismatch:", this->helicopterOwner, '/', this->pursuit);
			}

			this->helicopterOwner   = this->pursuit;
			this->helicopterVehicle = Globals::GetVehicleName(copVehicle);
		}


		void RelinquishOwnership() const
		{
			if (this->IsOwner())
			{
				this->helicopterOwner   = 0x0;
				this->helicopterVehicle = nullptr;
			}
		}


		void ProcessNewHelicopter(const address copVehicle)
		{
			this->TakeOwnership(copVehicle);

			hasLimitedFuel = fuelTimes.isEnableds.current;

			if (hasLimitedFuel)
				this->SetFuelTime(fuelTimes.GetRandomValue());

			this->spawnTimer.LoadInterval(lostRejoinDelays);
			this->minRejoinFuelTime = minRejoinFuelTimes.current;

			maxBailoutFuelTime = 8.f;

			if (this->spawnTimer.IsIntervalEnabled() and hasLimitedFuel)
				maxBailoutFuelTime = std::min<float>(lostRejoinDelays.minValues.current + this->minRejoinFuelTime, maxBailoutFuelTime);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL] Helicopter", (this->helicopterStatus != Status::PENDING) ? "respawned" : "spawned");
		}


		void MakeSpawnAttempt() const
		{
			if (this->helicopterOwner and (not this->IsOwner())) return;

			if (this->helicopterObject)            return;
			if (not this->isPlayerPursuit)         return;
			if (not this->spawnTimer.HasExpired()) return;

			if (Globals::copManager)
			{ 
				const auto SpawnHelicopter = reinterpret_cast<bool (__thiscall*)(address, address)>(0x4269A0);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Requesting helicopter");

				SpawnHelicopter(Globals::copManager, this->pursuit); // usually takes 1-2 attempts to succeed
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid AICopManager pointer");
		}


		static float* GetFuelTimePointer()
		{ 
			return (HelicopterManager::helicopterObject) ? reinterpret_cast<float*>(HelicopterManager::helicopterObject + 0x7D8) : nullptr;
		}


		void SetFuelTime(const float amount) const
		{
			if (not this->IsOwner()) return;

			float* const fuelTime = this->GetFuelTimePointer();

			if (fuelTime)
			{
				*fuelTime = amount;

				if constexpr (Globals::loggingEnabled)
				{
					if (hasLimitedFuel)
						Globals::logger.Log(this->pursuit, "[HEL] Fuel time:", amount);
				}
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid fuel pointer");
		}


		void UpdateSpawnTimer()
		{
			if (not this->isPlayerPursuit) return;

			const char* timerName = nullptr;

			switch (this->helicopterStatus)
			{
			case Status::PENDING:
				timerName = "First spawn";
				this->spawnTimer.LoadInterval(firstSpawnDelays);
				break;

			case Status::EXPIRED:
				timerName = "Fuel respawn";
				this->spawnTimer.LoadInterval(fuelRespawnDelays);
				break;

			case Status::WRECKED:
				timerName = "Wreck respawn";
				this->spawnTimer.LoadInterval(wreckRespawnDelays);
				break;

			case Status::LOST:
				timerName = "Lost respawn";
				this->spawnTimer.LoadInterval(lostRespawnDelays);
				break;

			default:
				return; // e.g. Status::REJOINING
			}

			if (not this->spawnTimer.IsSet())
				this->spawnTimer.Start();

			// With logging disabled, the compiler optimises "timerName" away
			if constexpr (Globals::loggingEnabled)
			{
				if (this->spawnTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[HEL]", timerName, "in", this->spawnTimer.GetTimeLeft());

				else
					Globals::logger.Log(this->pursuit, "[HEL]", timerName, "suspended");
			}
		}



	public:

		explicit HelicopterManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "HelicopterManager");
		}


		~HelicopterManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "HelicopterManager");

			this->RelinquishOwnership();
		}


		void UpdateOnGameplay() override 
		{
			this->MakeSpawnAttempt();
		}


		void UpdateOnHeatChange() override 
		{
			this->UpdateSpawnTimer();
		}


		void UpdateOncePerPursuit() override
		{
			this->VerifyPursuit();
			this->UpdateSpawnTimer();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel != CopLabel::HELICOPTER) return;

			this->spawnTimer.Stop();

			if (this->helicopterStatus == Status::REJOINING)
			{
				if (not this->IsOwner())
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log("WARNING: [HEL] Expected ownership in", this->pursuit);

					this->ProcessNewHelicopter(copVehicle);
				}
				else
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[HEL] Helicopter rejoined");

					this->SetFuelTime(this->fuelTimeOnRejoin);
					--(this->numHelicoptersDeployed);
				}
			}
			else this->ProcessNewHelicopter(copVehicle);
			
			this->helicopterStatus = Status::ACTIVE;

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

			if (Globals::IsVehicleDestroyed(copVehicle)) // destroyed
			{
				this->helicopterStatus = Status::WRECKED;
				this->RelinquishOwnership();
			}
			else // not destroyed
			{
				const float* const fuelTime = this->GetFuelTimePointer();

				if (not (fuelTime and (*fuelTime > 0.f))) // expired
				{
					this->helicopterStatus = Status::EXPIRED;
					this->RelinquishOwnership();
				}
				else if (this->spawnTimer.IsIntervalEnabled()) // lost, rejoining enabled
				{
					this->spawnTimer.Start(); // contains rejoin parameters

					const float rejoinDelay = this->spawnTimer.GetLength();
					
					if constexpr (Globals::loggingEnabled)
					{
						if (hasLimitedFuel)
							Globals::logger.Log(this->pursuit, "[HEL] Fuel at despawn:", *fuelTime);
					}

					this->fuelTimeOnRejoin = *fuelTime;

					if (hasLimitedFuel)
						this->fuelTimeOnRejoin -= rejoinDelay;

					if (this->fuelTimeOnRejoin >= this->minRejoinFuelTime) // sufficient fuel to rejoin
					{
						this->helicopterStatus = Status::REJOINING;

						if constexpr (Globals::loggingEnabled)
						{
							if (hasLimitedFuel)
								Globals::logger.Log(this->pursuit, "[HEL] Rejoining in", rejoinDelay, "for", this->fuelTimeOnRejoin);

							else
								Globals::logger.Log(this->pursuit, "[HEL] Rejoining in", rejoinDelay);
						}

						skipBailoutSpeech = true;
					}
					else // insufficient fuel to rejoin
					{
						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[HEL] Insufficient fuel to rejoin");

						this->helicopterStatus = Status::LOST;
						this->RelinquishOwnership();
					}
				}
				else // lost, rejoining disabled
				{
					this->helicopterStatus = Status::LOST;
					this->RelinquishOwnership();
				}
			}

			this->UpdateSpawnTimer();
		}


		static const char* GetHelicopterVehicle()
		{
			if (not featureEnabled) return nullptr;

			if (HelicopterManager::helicopterOwner)
			{
				if (HelicopterManager::helicopterVehicle)
					return HelicopterManager::helicopterVehicle;

				else if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [HEL] Invalid vehicle pointer");
			}

			return helicopterVehicles.current;
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address fuelUpdateEntrance = 0x423519;
	constexpr address fuelUpdateExit     = 0x423523;

	// Updates the amount of fuel remaining
	__declspec(naked) void FuelUpdate()
	{
		__asm
		{
			cmp byte ptr hasLimitedFuel, 0x1
			jne conclusion // unlimited fuel

			// Execute original code and resume
			fsub dword ptr [esp + 0x8]  // time delta
			fst dword ptr [esi + 0x7D8] // helicopter fuel

			conclusion:
			jmp dword ptr fuelUpdateExit
		}
	}



	constexpr address defaultFuelEntrance = 0x42AD9B;
	constexpr address defaultFuelExit     = 0x42ADA1;

	// Sets the default helicopter fuel
	__declspec(naked) void DefaultFuel()
	{
		static constexpr float fuelTime = std::numeric_limits<float>::max();

		__asm
		{
			mov ecx, dword ptr fuelTime
			mov dword ptr [esi + 0x34], ecx // helicopter fuel

			jmp dword ptr defaultFuelExit
		}
	}



	constexpr address earlyBailoutEntrance = 0x717C00;
	constexpr address earlyBailoutExit     = 0x717C06;

	// Decides whether to request a bailout announcement over the radio
	__declspec(naked) void EarlyBailout()
	{
		static constexpr address earlyBailoutSkip = 0x717C34;

		__asm
		{
			cmp byte ptr skipBailoutSpeech, 0x1
			je skip // speech disabled

			// Execute original code and resume
			sub esp, 0x8
			push esi
			mov esi, ecx

			jmp dword ptr earlyBailoutExit

			skip:
			jmp dword ptr earlyBailoutSkip
		}
	}



	constexpr address rammingCooldownEntrance = 0x4128B2;
	constexpr address rammingCooldownExit     = 0x4128B9;

	// Sets the cooldown for HeliStrategy 2 ramming attempts
	__declspec(naked) void RammingCooldown()
	{
		__asm
		{
			mov ecx, offset rammingCooldowns
			call HeatParameters::Interval<float>::GetRandomValue
			fstp dword ptr [esi + 0x64] // HeliStrategy 2 cooldown

			jmp dword ptr rammingCooldownExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Helicopter.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Helicopter:Vehicle", helicopterVehicles);

		HeatParameters::Parse(parser, "Helicopter:FirstSpawn",   firstSpawnDelays);
		HeatParameters::Parse(parser, "Helicopter:FuelRespawn",  fuelRespawnDelays);
		HeatParameters::Parse(parser, "Helicopter:WreckRespawn", wreckRespawnDelays);
		HeatParameters::Parse(parser, "Helicopter:LostRespawn",  lostRespawnDelays);
		HeatParameters::Parse(parser, "Helicopter:LostRejoin",   lostRejoinDelays, minRejoinFuelTimes);
		HeatParameters::Parse(parser, "Helicopter:FuelTime",     fuelTimes);

		HeatParameters::Parse(parser, "Helicopter:Ramming", rammingCooldowns);

		// Code modifications 
		MemoryTools::Write<float*>(&maxBailoutFuelTime, {0x709F9F, 0x7078B0});

		MemoryTools::MakeRangeNOP(0x419160, 0x41916E); // roadblock-induced lobotomy

		MemoryTools::MakeRangeJMP(FuelUpdate,      fuelUpdateEntrance,      fuelUpdateExit);
		MemoryTools::MakeRangeJMP(DefaultFuel,     defaultFuelEntrance,     defaultFuelExit);
		MemoryTools::MakeRangeJMP(EarlyBailout,    earlyBailoutEntrance,    earlyBailoutExit);
		MemoryTools::MakeRangeJMP(RammingCooldown, rammingCooldownEntrance, rammingCooldownExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [HEL] HelicopterOverrides");

		// With logging disabled, the compiler optimises the boolean and the string literal away
		const bool allValid = HeatParameters::ValidateVehicles("Helicopters", helicopterVehicles, Globals::Class::CHOPPER);

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.LogLongIndent("All vehicles valid");
		}
	}



	void LogHeatReport()
	{
		if (
			firstSpawnDelays.isEnableds.current
			or fuelRespawnDelays.isEnableds.current
			or wreckRespawnDelays.isEnableds.current
			or lostRejoinDelays.isEnableds.current
			or fuelTimes.isEnableds.current
		   )
		{
			Globals::logger.Log("    HEAT [HEL] HelicopterOverrides");

			helicopterVehicles.Log("helicopterVehicle       ");
			rammingCooldowns  .Log("rammingCooldown         ");

			fuelTimes         .Log("fuelTime                ");
			firstSpawnDelays  .Log("firstSpawnDelay         ");
			fuelRespawnDelays .Log("fuelRespawnDelays       ");
			wreckRespawnDelays.Log("wreckRespawnDelay       ");
			lostRespawnDelays .Log("lostRespawnDelay        ");

			if (lostRejoinDelays.isEnableds.current)
			{
				lostRejoinDelays  .Log("lostRejoinDelay         ");
				minRejoinFuelTimes.Log("minRejoinFuelTime       ");
			}
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		helicopterVehicles.SetToHeat(isRacing, heatLevel);
		rammingCooldowns  .SetToHeat(isRacing, heatLevel);

		fuelTimes         .SetToHeat(isRacing, heatLevel);
		firstSpawnDelays  .SetToHeat(isRacing, heatLevel);
		fuelRespawnDelays .SetToHeat(isRacing, heatLevel);
		wreckRespawnDelays.SetToHeat(isRacing, heatLevel);
		lostRespawnDelays .SetToHeat(isRacing, heatLevel);

		lostRejoinDelays  .SetToHeat(isRacing, heatLevel);
		minRejoinFuelTimes.SetToHeat(isRacing, heatLevel);
		
		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}