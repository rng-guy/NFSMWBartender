#pragma once

#include <limits>
#include <algorithm>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace HelicopterOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Value<const char*> helicopterVehicle("copheli");

	constinit HeatParameters::OptionalInterval<float> firstSpawnDelay({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> fuelRespawnDelay({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> wreckRespawnDelay({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> lostRespawnDelay({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> lostRejoinDelay  ({1.f}); // seconds
	constinit HeatParameters::OptionalValue   <float> minRejoinFuelTime({1.f}); // seconds

	constinit HeatParameters::OptionalInterval<float> fuelTime({1.f}); // seconds

	constinit HeatParameters::Interval<float> chaseSpawnDistance(250.f, 250.f, {0.f, 450.f}); // metres

	constinit HeatParameters::Interval<float> searchSpawnDistance(250.f, 250.f, {0.f, 450.f}); // metres

	constinit HeatParameters::Value<bool> affectedByRoadblock(true);

	constinit HeatParameters::Interval<float> rammingCooldown(8.f, 8.f, {1.f}); // seconds

	// Code caves 
	bool hasLimitedFuel    = false;
	bool skipBailoutSpeech = false;

	float maxBailoutFuelTime = 8.f; // seconds





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::PursuitReaction
	{
	private:

		// Internal enum
		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST,
			REJOINING
		};


	private:
		
		bool   isPlayerPursuit  = false;
		Status helicopterStatus = Status::PENDING;

		PursuitFeatures::IntervalTimer spawnTimer;

		float fuelTimeOnRejoin    = 0.f;  // seconds
		float minFuelTimeToRejoin = 10.f; // seconds

		volatile int& numHelisDeployed = AsVolatile<int>(this->pursuit + 0x150);

		inline static constinit address     helicopterOwner = 0x0;
		inline static constinit const char* helicopterName  = nullptr;

		inline static const volatile address& helicopterObject = AsVolatile<address>(0x90D61C);


		void VerifyPursuit()
		{
			this->isPlayerPursuit = Globals::IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL]", (this->isPlayerPursuit) ? "Is" : "Not", "player pursuit");
		}


		[[nodiscard]] bool IsOwner() const
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

			const char* copName = Globals::GetVehicleName(copVehicle);
			HeatParameters::MakePersistentString(copName);

			this->helicopterOwner = this->pursuit;
			this->helicopterName  = copName;
		}


		void RelinquishOwnership() const
		{
			if (this->IsOwner())
			{
				this->helicopterOwner = 0x0;
				this->helicopterName  = nullptr;
			}
		}


		void ProcessNewHelicopter(const address copVehicle)
		{
			this->TakeOwnership(copVehicle);

			hasLimitedFuel = fuelTime.isEnabled.current;
			this->spawnTimer.LoadInterval(lostRejoinDelay);

			if (hasLimitedFuel)
				this->SetFuelTime(fuelTime.GetRandomValue());

			maxBailoutFuelTime = 8.f; // vanilla value

			if (this->spawnTimer.IsIntervalEnabled()) // rejoining enabled
			{
				this->minFuelTimeToRejoin = minRejoinFuelTime.value.current;

				if (hasLimitedFuel)
					maxBailoutFuelTime = std::min<float>(lostRejoinDelay.min.current + this->minFuelTimeToRejoin, maxBailoutFuelTime);
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[HEL] Helicopter", (this->helicopterStatus != Status::PENDING) ? "respawned" : "spawned");
		}


		void CallOutHelicopterSpawn() const
		{
			if (not Globals::IsInCooldownMode(this->pursuit)) return;

			const address soundAI = AsVolatile<address>(0x993CC8);
			if (not soundAI) return; // should never happen

			const address helicopterActor = AsVolatile<address>(soundAI + 0xE0);
			if (not helicopterActor) return; // should never happen

			const auto CallOutSweep = AsFunction<void __thiscall (address)>(0x717D40);
			CallOutSweep(helicopterActor); // requests radio callout for helicopter search
		}


		void MakeSpawnAttempt() const
		{
			if (this->helicopterOwner and (not this->IsOwner())) return;

			if (this->helicopterObject)            return;
			if (not this->isPlayerPursuit)         return;
			if (not this->spawnTimer.HasExpired()) return;

			if (Globals::copManager)
			{ 
				const auto SpawnHelicopter = AsFunction<bool __thiscall (address, address)>(0x4269A0);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[HEL] Requesting helicopter");

				if (SpawnHelicopter(Globals::copManager, this->pursuit))
					this->CallOutHelicopterSpawn();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [HEL] Invalid AICopManager pointer");
		}


		[[nodiscard]] static volatile float* GetFuelTimePointer()
		{ 
			if (not HelicopterManager::helicopterObject) return nullptr; // should never happen
			return AsPointer<float>(HelicopterManager::helicopterObject + 0x7D8);
		}


		void SetFuelTime(const float amount) const
		{
			if (not this->IsOwner()) return;

			auto* const fuelTime = this->GetFuelTimePointer();

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

			std::string_view timerName;

			switch (this->helicopterStatus)
			{
			case Status::PENDING:
				timerName = "First spawn";
				this->spawnTimer.LoadInterval(firstSpawnDelay);
				break;

			case Status::EXPIRED:
				timerName = "Fuel respawn";
				this->spawnTimer.LoadInterval(fuelRespawnDelay);
				break;

			case Status::WRECKED:
				timerName = "Wreck respawn";
				this->spawnTimer.LoadInterval(wreckRespawnDelay);
				break;

			case Status::LOST:
				timerName = "Lost respawn";
				this->spawnTimer.LoadInterval(lostRespawnDelay);
				break;

			default:
				return; // ACTIVE, REJOINING
			}

			if (not this->spawnTimer.IsSet())
				this->spawnTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				if (this->spawnTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[HEL]", timerName, "in", this->spawnTimer.GetTimeLeft());

				else
					Globals::logger.Log(this->pursuit, "[HEL]", timerName, "suspended");
			}
		}


	public:

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


		explicit HelicopterManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "HelicopterManager");
		}


		~HelicopterManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "HelicopterManager");

			this->RelinquishOwnership();
		}


		void ReactToGameplay() override 
		{
			this->MakeSpawnAttempt();
		}


		void ReactToHeatStateUpdate() override 
		{
			this->UpdateSpawnTimer();
		}


		void ReactToPursuitStartWithDelay() override
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
					--(this->numHelisDeployed);
				}
			}
			else this->ProcessNewHelicopter(copVehicle);
			
			this->helicopterStatus = Status::ACTIVE;
			skipBailoutSpeech      = false;
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
				const auto* const fuelTime = this->GetFuelTimePointer();

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

					this->fuelTimeOnRejoin = (hasLimitedFuel) ? (*fuelTime - rejoinDelay) : *fuelTime;

					if (this->fuelTimeOnRejoin >= this->minFuelTimeToRejoin) // sufficient fuel to rejoin
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


		[[nodiscard]] static const char* GetHelicopterName()
		{
			if (not anyFeatureEnabled) return nullptr;

			if (HelicopterManager::helicopterOwner)
			{
				if (HelicopterManager::helicopterName)
					return HelicopterManager::helicopterName;

				else if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [HEL] Invalid name pointer");
			}

			return helicopterVehicle.current;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] float __fastcall GetSpawnDistance(const address pursuit)
	{
		const float distance = ((Globals::IsInCooldownMode(pursuit)) ? searchSpawnDistance : chaseSpawnDistance).GetRandomValue();

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log<2>("Spawn distance:", distance);

		return distance;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address fuelUpdateEntrance = 0x423519;
	constexpr address fuelUpdateExit     = 0x423523;

	// Updates the amount of fuel remaining
	__declspec(naked) void FuelUpdate()
	{
		__asm
		{
			cmp byte ptr [hasLimitedFuel], 1
			jne conclusion // unlimited fuel

			// Execute original code and resume
			fsub dword ptr [esp + 0x8]  // time delta
			fst dword ptr [esi + 0x7D8] // helicopter fuel

			conclusion:
			jmp dword ptr [fuelUpdateExit]
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
			mov ecx, dword ptr [fuelTime]
			mov dword ptr [esi + 0x34], ecx // helicopter fuel

			jmp dword ptr [defaultFuelExit]
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
			cmp byte ptr [skipBailoutSpeech], 1
			je skip // speech disabled

			// Execute original code and resume
			sub esp, 0x8
			push esi
			mov esi, ecx

			jmp dword ptr [earlyBailoutExit]

			skip:
			jmp dword ptr [earlyBailoutSkip]
		}
	}



	constexpr address spawnDistanceEntrance = 0x426ABF;
	constexpr address spawnDistanceExit     = 0x426AC4;

	// Sets the spawn distance to the pursuit target
	__declspec(naked) void SpawnDistance()
	{
		__asm
		{
			mov ecx, ebp
			call GetSpawnDistance // ecx: pursuit

			push eax
			fstp dword ptr [esp]

			jmp dword ptr [spawnDistanceExit]
		}
	}



	constexpr address roadblockCheckEntrance = 0x419160;
	constexpr address roadblockCheckExit     = 0x419168;

	// Controls whether roadblocks affect helicopter behaviour
	__declspec(naked) void RoadblockCheck()
	{
		__asm
		{
			cmp byte ptr [affectedByRoadblock.current], 0
			je conclusion // helicopter unaffected

			// Execute original code and resume
			mov eax, dword ptr [ecx + 0xCC]
			test eax, eax

			conclusion:
			jmp dword ptr [roadblockCheckExit]
		}
	}



	constexpr address rammingCooldownEntrance = 0x4128B2;
	constexpr address rammingCooldownExit     = 0x4128B9;

	// Sets the cooldown for HeliStrategy 2 ramming attempts
	__declspec(naked) void RammingCooldown()
	{
		__asm
		{
			mov ecx, offset rammingCooldown
			call HeatParameters::Interval<float>::GetRandomValue
			fstp dword ptr [esi + 0x64] // HeliStrategy 2 cooldown

			jmp dword ptr [rammingCooldownExit]
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [HEL] HelicopterOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Helicopter.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Helicopter:Vehicle", helicopterVehicle);

		HeatParameters::Parse(parser, "Helicopter:FirstSpawn", firstSpawnDelay);

		HeatParameters::Parse(parser, "Helicopter:FuelRespawn", fuelRespawnDelay);

		HeatParameters::Parse(parser, "Helicopter:WreckRespawn", wreckRespawnDelay);

		HeatParameters::Parse(parser, "Helicopter:LostRespawn", lostRespawnDelay);

		HeatParameters::Parse(parser, "Helicopter:LostRejoin", lostRejoinDelay, minRejoinFuelTime);

		HeatParameters::Parse(parser, "Helicopter:FuelTime", fuelTime);

		HeatParameters::Parse(parser, "Helicopter:Chasing", chaseSpawnDistance);

		HeatParameters::Parse(parser, "Helicopter:Searching", searchSpawnDistance);

		HeatParameters::Parse(parser, "Helicopter:Roadblocks", affectedByRoadblock);

		HeatParameters::Parse(parser, "Helicopter:Ramming", rammingCooldown);

		// Check and make vehicle names persistent
		if (HeatParameters::ResolveVehicleNames("Helicopters", helicopterVehicle, Globals::IsVehicleTypeChopper))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("All vehicles valid");
		}

		// Code modifications 
		MemoryTools::Write<float*>(&maxBailoutFuelTime, {0x709F9F, 0x7078B0});

		MemoryTools::MakeRangeJMP<fuelUpdateEntrance,      fuelUpdateExit>     (FuelUpdate);
		MemoryTools::MakeRangeJMP<defaultFuelEntrance,     defaultFuelExit>    (DefaultFuel);
		MemoryTools::MakeRangeJMP<earlyBailoutEntrance,    earlyBailoutExit>   (EarlyBailout);
		MemoryTools::MakeRangeJMP<spawnDistanceEntrance,   spawnDistanceExit>  (SpawnDistance);
		MemoryTools::MakeRangeJMP<roadblockCheckEntrance,  roadblockCheckExit> (RoadblockCheck);
		MemoryTools::MakeRangeJMP<rammingCooldownEntrance, rammingCooldownExit>(RammingCooldown);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		if (
			firstSpawnDelay     .isEnabled.current
			or fuelRespawnDelay .isEnabled.current
			or wreckRespawnDelay.isEnabled.current
			or lostRejoinDelay  .isEnabled.current
			or fuelTime         .isEnabled.current
		   )
		{
			Globals::logger.Log("    HEAT [HEL] HelicopterOverrides");

			helicopterVehicle.Log("helicopterVehicle       ");
			
			firstSpawnDelay.Log("firstSpawnDelay         ");

			fuelRespawnDelay.Log("fuelRespawnDelays       ");

			wreckRespawnDelay.Log("wreckRespawnDelay       ");

			lostRespawnDelay.Log("lostRespawnDelay        ");

			lostRejoinDelay  .Log("lostRejoinDelay         ");
			minRejoinFuelTime.Log("minRejoinFuelTime       ");

			fuelTime.Log("fuelTime                ");
		}

		chaseSpawnDistance.Log("chaseSpawnDistance      ");

		searchSpawnDistance.Log("searchSpawnDistance     ");

		affectedByRoadblock.Log("isAffectedByRoadblock   ");

		rammingCooldown.Log("rammingCooldown         ");
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		helicopterVehicle.SetToHeatState(isRacing, heatLevel);
		
		firstSpawnDelay.SetToHeatState(isRacing, heatLevel);

		fuelRespawnDelay.SetToHeatState(isRacing, heatLevel);

		wreckRespawnDelay.SetToHeatState(isRacing, heatLevel);

		lostRespawnDelay.SetToHeatState(isRacing, heatLevel);

		lostRejoinDelay  .SetToHeatState(isRacing, heatLevel);
		minRejoinFuelTime.SetToHeatState(isRacing, heatLevel);

		fuelTime.SetToHeatState(isRacing, heatLevel);

		chaseSpawnDistance.SetToHeatState(isRacing, heatLevel);

		searchSpawnDistance.SetToHeatState(isRacing, heatLevel);

		affectedByRoadblock.SetToHeatState(isRacing, heatLevel);

		rammingCooldown.SetToHeatState(isRacing, heatLevel);
		
		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}
}