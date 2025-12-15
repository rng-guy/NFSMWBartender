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
	HeatParameters::Pair<std::string> helicopterVehicles("copheli");
	HeatParameters::Pair<float>       rammingCooldowns  (8.f);       // seconds

	HeatParameters::OptionalInterval<float> firstSpawnDelays;
	HeatParameters::OptionalInterval<float> fuelRespawnDelays;
	HeatParameters::OptionalInterval<float> wreckRespawnDelays;

	HeatParameters::OptionalInterval<float> lostRejoinDelays;
	HeatParameters::Pair            <float> minRejoinFuelTimes(10.f); // seconds

	HeatParameters::OptionalInterval<float> fuelTimes;

	// Code caves 
	bool hasLimitedFuel    = false;
	bool skipBailoutSpeech = false;

	float maxBailoutFuelTime = 8.f; // seconds





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::PursuitReaction
	{
	private:
		
		bool isPlayerPursuit = false;

		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST
		};

		Status helicopterStatus = Status::PENDING;

		PursuitFeatures::IntervalTimer spawnTimer;

		int& numHelicoptersDeployed = *reinterpret_cast<int*>(this->pursuit + 0x150);

		inline static address     helicopterOwner   = 0x0;
		inline static const char* helicopterVehicle = nullptr;

		inline static float fuelTimeOnRejoin  = 0.f;  // seconds
		inline static float minRejoinFuelTime = 10.f;

		inline static const address& helicopterObject = *reinterpret_cast<address*>(0x90D61C);


		void VerifyPursuit()
		{
			this->isPlayerPursuit = Globals::IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Confirmed" : "Not", "player pursuit");
		}


		bool IsOwner() const
		{
			return (this->helicopterOwner == this->pursuit);
		}


		void TransferOwnership(const address copVehicle) const
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


		void LockInRejoinParameters()
		{
			if (not this->IsOwner()) return;

			this->spawnTimer.LoadInterval(lostRejoinDelays);
			hasLimitedFuel = fuelTimes.isEnableds.current;

			if (this-spawnTimer.IsIntervalEnabled() and hasLimitedFuel)
			{
				this->minRejoinFuelTime = minRejoinFuelTimes.current;
				maxBailoutFuelTime      = std::min<float>(lostRejoinDelays.minValues.current + this->minRejoinFuelTime, 8.f);
			}
			else maxBailoutFuelTime = 8.f;
		}


		bool HasSpawnedBefore() const
		{
			return (this->helicopterStatus != Status::PENDING);
		}


		bool IsRejoining() const
		{
			return (this->helicopterStatus == Status::LOST);
		}


		void MakeSpawnAttempt() const
		{
			if (this->helicopterOwner and (not this->IsOwner())) return;

			if (this->helicopterObject)            return;
			if (not this->isPlayerPursuit)         return;
			if (not this->spawnTimer.HasExpired()) return;

			if (Globals::copManager)
			{ 
				static const auto SpawnHelicopter = reinterpret_cast<bool (__thiscall*)(address, address)>(0x4269A0);

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

			switch (this->helicopterStatus)
			{
			case Status::PENDING:
				this->spawnTimer.LoadInterval(firstSpawnDelays);
				break;

			case Status::EXPIRED:
				this->spawnTimer.LoadInterval(fuelRespawnDelays);
				break;

			case Status::WRECKED:
				this->spawnTimer.LoadInterval(wreckRespawnDelays);
				break;

			default:
				return;
			}

			if (not this->spawnTimer.IsSet())
				this->spawnTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				const char* actionName = nullptr;

				switch (this->helicopterStatus)
				{
				case Status::PENDING:
					actionName = "First spawn";
					break;

				case Status::EXPIRED:
					actionName = "Fuel respawn";
					break;

				case Status::WRECKED:
					actionName = "Wreck respawn";
				}

				if (actionName)
				{
					if (this->spawnTimer.IsIntervalEnabled())
						Globals::logger.Log(this->pursuit, "[HEL]", actionName, "in", this->spawnTimer.GetTimeLeft());

					else
						Globals::logger.Log(this->pursuit, "[HEL]", actionName, "suspended");
				}
				else Globals::logger.Log("WARNING: [HEL] Invalid status in timer update", this->helicopterStatus, "in", this->pursuit);
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

			if (not (this->IsOwner() and this->IsRejoining())) // is new helicopter
			{
				this->TransferOwnership(copVehicle);

				if (this->IsRejoining())
				{
					this->helicopterStatus = Status::EXPIRED;

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log("WARNING: [HEL] Expected ownership in", this->pursuit);
				}
				
				this->LockInRejoinParameters();
			}
			
			if (this->IsRejoining())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Helicopter rejoined");

				this->SetFuelTime(this->fuelTimeOnRejoin);
				--(this->numHelicoptersDeployed);
			}
			else
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Helicopter", (this->HasSpawnedBefore()) ? "respawned" : "spawned");

				if (hasLimitedFuel)
					this->SetFuelTime(fuelTimes.GetRandomValue());
			}

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

			const float* const fuelTime = this->GetFuelTimePointer();

			if (Globals::IsVehicleDestroyed(copVehicle))
			{
				this->helicopterStatus = Status::WRECKED;
				this->RelinquishOwnership();
			}
			else if (not (fuelTime and (*fuelTime > 0.f)))
			{
				this->helicopterStatus = Status::EXPIRED;
				this->RelinquishOwnership();
			}
			else
			{
				if constexpr (Globals::loggingEnabled)
				{
					if (hasLimitedFuel)
						Globals::logger.Log(this->pursuit, "[HEL] Fuel at despawn:", *fuelTime);
				}

				this->spawnTimer.Start(); // holds rejoin-delay interval

				if (this->spawnTimer.IsIntervalEnabled())
				{
					if (hasLimitedFuel)
					{
						this->fuelTimeOnRejoin = *fuelTime - this->spawnTimer.GetLength();
						this->helicopterStatus = (this->fuelTimeOnRejoin >= this->minRejoinFuelTime) ? Status::LOST : Status::EXPIRED;
					}
					else
					{
						this->fuelTimeOnRejoin = *fuelTime;
						this->helicopterStatus = Status::LOST;
					}
				}
				else this->helicopterStatus = Status::EXPIRED;

				if (this->IsRejoining())
				{
					if constexpr (Globals::loggingEnabled)
					{
						if (hasLimitedFuel)
							Globals::logger.Log(this->pursuit, "[HEL] Rejoining in", this->spawnTimer.GetLength(), "for", this->fuelTimeOnRejoin);

						else
							Globals::logger.Log(this->pursuit, "[HEL] Rejoining in", this->spawnTimer.GetLength());
					}

					skipBailoutSpeech = true;
				}
				else
				{
					if constexpr (Globals::loggingEnabled)
					{
						if (this->spawnTimer.IsIntervalEnabled())
							Globals::logger.Log(this->pursuit, "[HEL] Insufficient fuel to rejoin");

						else
							Globals::logger.Log(this->pursuit, "[HEL] Rejoining disabled");
					}

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
		HeatParameters::ParseOptional<float>(parser, "Helicopter:FirstSpawn", {firstSpawnDelays, 1.f});
		if (not firstSpawnDelays.AnyEnabled()) return false; // no helicopters can ever spawn

		HeatParameters::ParseOptional<float>(parser, "Helicopter:FuelRespawn",  {fuelRespawnDelays , 1.f});
		HeatParameters::ParseOptional<float>(parser, "Helicopter:WreckRespawn", {wreckRespawnDelays, 1.f});
		HeatParameters::ParseOptional<float>(parser, "Helicopter:FuelTime",     {fuelTimes,          1.f});

		HeatParameters::ParseOptional<float, float>(parser, "Helicopter:LostRejoin", {lostRejoinDelays, 1.f}, {minRejoinFuelTimes, 1.f});

		HeatParameters::Parse<std::string>(parser, "Helicopter:Vehicle", {helicopterVehicles});
		HeatParameters::Parse<float>      (parser, "Helicopter:Ramming", {rammingCooldowns, 1.f});

		// Code modifications 
		MemoryTools::Write<float*>(&maxBailoutFuelTime, {0x709F9F, 0x7078B0});

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

			firstSpawnDelays  .Log("firstSpawnDelay         ");
			fuelRespawnDelays .Log("fuelRespawnDelays       ");
			wreckRespawnDelays.Log("wreckRespawnDelay       ");

			if (lostRejoinDelays.isEnableds.current)
			{
				lostRejoinDelays  .Log("lostRejoinDelay         ");
				minRejoinFuelTimes.Log("minRejoinFuelTime       ");
			}

			fuelTimes.Log("fuelTime                ");
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

		firstSpawnDelays  .SetToHeat(isRacing, heatLevel);
		fuelRespawnDelays .SetToHeat(isRacing, heatLevel);
		wreckRespawnDelays.SetToHeat(isRacing, heatLevel);
		lostRejoinDelays  .SetToHeat(isRacing, heatLevel);
		minRejoinFuelTimes.SetToHeat(isRacing, heatLevel);
		fuelTimes         .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}