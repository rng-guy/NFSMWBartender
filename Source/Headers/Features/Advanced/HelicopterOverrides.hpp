#pragma once

#include <optional>
#include <algorithm>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/HeatParameters.hpp"
#include "../../Common/PersistentStrings.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "PursuitFeatures.hpp"



namespace HelicopterOverrides
{
	// Feature data ---------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[HEL]";
	constexpr Globals::LogLiteral logName = "HelicopterOverrides";

	// Heat parameters
	constinit HEAT_PARAMETER_VALUE(const char*, helicopterVehicle, "copheli");

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, firstSpawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, fuelRespawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, wreckRespawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, lostRespawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, lostRejoinDelay,   {1.f}); // seconds
	constinit OPTIONAL_HEAT_PARAMETER_VALUE   (float, minRejoinFuelTime, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, fuelTime, {1.f}); // seconds

	constinit HEAT_PARAMETER_INTERVAL(float, chaseSpawnDistance, 250.f, 250.f, {0.f, 450.f}); // metres

	constinit HEAT_PARAMETER_INTERVAL(float, searchSpawnDistance, 250.f, 250.f, {0.f, 450.f}); // metres

	constinit HEAT_PARAMETER_VALUE(bool, affectedByRoadblock, true);

	constinit HEAT_PARAMETER_INTERVAL(float, rammingCooldown, 8.f, 8.f, {1.f}); // seconds

	// ASM detours
	bool hasLimitedFuel    = false;
	bool skipBailoutSpeech = false;

	float maxBailoutFuelTime = 8.f; // seconds





	// HelicopterManager class ----------------------------------------------------------------------------------------------------------------------

	class HelicopterManager : public PursuitFeatures::Reaction
	{
	private: // types

		enum class Status
		{
			PENDING,
			ACTIVE,
			EXPIRED,
			WRECKED,
			LOST,
			REJOINING
		};


		struct RejoinContext
		{
			address pursuit;

			const char* helicopterName;

			float minRejoinDelay;
			float maxRejoinDelay;

			float minRejoinFuelTime;

			float fuelTimeOnRejoin = 0.f;
		};

		
	private: // members
		
		bool isPlayerPursuit = false;

		Status helicopterStatus = Status::PENDING;

		PursuitFeatures::IntervalTimer spawnTimer;

		int& numHelisDeployed = AsReference<int>(this->pursuit + 0x150); // helicopters

		bool& searchSpawnAllowed = AsReference<bool>(this->pursuit + 0xD4);

		inline static constinit std::optional<RejoinContext> rejoinContext;

		inline static const address& helicopter = AsReference<address>(0x90D61C);

		inline static constexpr Globals::LogLiteral name = "HelicopterManager";


	private: // methods

		void VerifyPursuit()
		{
			this->isPlayerPursuit = Globals::IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, (this->isPlayerPursuit) ? "Is" : "Not", "player pursuit");
		}


		[[nodiscard]] bool HasRejoinContext() const
		{
			return (this->rejoinContext and (this->rejoinContext->pursuit == this->pursuit));
		}


		[[nodiscard]] bool IsBlockedByRejoining() const
		{
			return (this->rejoinContext and (this->rejoinContext->pursuit != this->pursuit));
		}


		[[nodiscard]] bool IsHelicopterRejoining() const
		{
			const bool hasStatus = (this->helicopterStatus == Status::REJOINING);

			if (hasStatus and (not this->HasRejoinContext()))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Rejoin-status mismatch in", this->pursuit);

				ASSERT_UNREACHABLE_THEN(return false);
			}

			return hasStatus;
		}


		[[nodiscard]] static float* GetFuelTimePointer()
		{ 
			return (HelicopterManager::helicopter) ? AsPointer<float>(HelicopterManager::helicopter + 0x7D8) : nullptr;
		}


		void SetFuelTime(const float amount) const
		{
			if (not hasLimitedFuel) return;

			float* const fuelTime = this->GetFuelTimePointer();

			if (not fuelTime)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid fuel pointer in", this->pursuit);

				ASSERT_UNREACHABLE_THEN(return);
			}

			*fuelTime = amount;

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Fuel time:", amount);
		}


		void ProcessNewHelicopter(const address copVehicle)
		{
			hasLimitedFuel     = fuelTime.isEnabled.current;
			maxBailoutFuelTime = 8.f; // vanilla value

			this->SetFuelTime(fuelTime.interval.GetRandomValue());

			// Update rejoin context
			if (lostRejoinDelay.isEnabled.current and minRejoinFuelTime.isEnabled.current)
			{
				const auto& rejoinDelay = lostRejoinDelay.interval;
				const float minFuelTime = minRejoinFuelTime.value.current;

				this->rejoinContext =
				{
					.pursuit           = this->pursuit,
					.helicopterName    = Globals::GetVehicleName(copVehicle),
					.minRejoinDelay    = rejoinDelay.min.current,
					.maxRejoinDelay    = rejoinDelay.max.current,
					.minRejoinFuelTime = minFuelTime
				};

				PersistentStrings::Make(this->rejoinContext->helicopterName);

				if (hasLimitedFuel)
					maxBailoutFuelTime = std::min<float>(rejoinDelay.min.current + minFuelTime, maxBailoutFuelTime);
			}
			else this->rejoinContext.reset();

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Helicopter", (this->helicopterStatus != Status::PENDING) ? "respawned" : "spawned");
		}


		[[nodiscard]] bool IsBlockedByCooldownMode() const
		{
			if (not Globals::IsPursuitInCooldownMode(this->pursuit)) return false;
			return (not (this->IsHelicopterRejoining() or this->searchSpawnAllowed));
		}


		void CallOutHelicopterSpawn() const
		{
			if (not Globals::IsPursuitInCooldownMode(this->pursuit)) return;

			const address soundAI = AsReference<address>(0x993CC8);
			ASSERT_CONDITION_THEN_IF_FALSE(soundAI, return);

			const address helicopterActor = AsReference<address>(soundAI + 0xE0);
			ASSERT_CONDITION_THEN_IF_FALSE(helicopterActor, return);

			const auto CallOutSweep = AsFunction<void __thiscall (address)>(0x717D40);
			CallOutSweep(helicopterActor); // requests radio callout for helicopter search
		}


		void MakeSpawnAttempt() const
		{
			if (this->helicopter)                  return;
			if (not this->isPlayerPursuit)         return;
			if (this->IsBlockedByRejoining())      return;
			if (not this->spawnTimer.HasExpired()) return;
			if (this->IsBlockedByCooldownMode())   return;

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Requesting helicopter");

			const auto SpawnHelicopter = AsFunction<bool __thiscall (address, address)>(0x4269A0);

			if (SpawnHelicopter(Globals::copManager, this->pursuit))
				this->CallOutHelicopterSpawn();
		}


		void UpdateSpawnTimer()
		{
			if (not this->isPlayerPursuit) return;

			Globals::LogLiteral spawnName;

			switch (this->helicopterStatus)
			{
			case Status::PENDING:
				spawnName = "First spawn";
				this->spawnTimer.LoadInterval(firstSpawnDelay);
				break;

			case Status::EXPIRED:
				spawnName = "Fuel respawn";
				this->spawnTimer.LoadInterval(fuelRespawnDelay);
				break;

			case Status::WRECKED:
				spawnName = "Wreck respawn";
				this->spawnTimer.LoadInterval(wreckRespawnDelay);
				break;

			case Status::LOST:
				spawnName = "Lost respawn";
				this->spawnTimer.LoadInterval(lostRespawnDelay);
				break;

			default:
				return; // ACTIVE, REJOINING
			}

			this->spawnTimer.SetStartTimestampIfNone();

			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogFull(this->pursuit, logTag, "New spawn timer");

				this->spawnTimer.Log(spawnName);
			}
		}


		void UpdateHelicopterStatus(const Status status)
		{
			this->helicopterStatus = status;

			switch (status)
			{
			case Status::ACTIVE:
				this->searchSpawnAllowed = false;
				skipBailoutSpeech        = false;
				break;

			case Status::EXPIRED:
			case Status::WRECKED:
			case Status::LOST:
				this->rejoinContext.reset();
				break;

			case Status::REJOINING:
				skipBailoutSpeech = true;
			}

			this->UpdateSpawnTimer();
		}


		[[nodiscard]] bool ScheduleRejoining(const float fuelTime)
		{
			if (not this->HasRejoinContext()) return false;

			auto& context = *(this->rejoinContext);

			// Set spawn timer for rejoining
			this->spawnTimer.SetStartTimestamp();

			this->spawnTimer.LoadInterval(context.minRejoinDelay, context.maxRejoinDelay);

			// Calculate rejoin fuel
			const auto rejoinDelay = this->spawnTimer.GetLength();

			if (not rejoinDelay)
			{
				this->spawnTimer.ClearStartTimestamp();

				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Disabled rejoin interval in", this->pursuit);

				ASSERT_UNREACHABLE_THEN(return false);
			}

			context.fuelTimeOnRejoin = fuelTime;

			if (hasLimitedFuel)
			{
				context.fuelTimeOnRejoin -= *rejoinDelay;

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Fuel at despawn:", fuelTime);
			}

			// Check rejoin conditions
			if (Globals::IsPursuitInCooldownMode(this->pursuit))
			{
				this->spawnTimer.ClearStartTimestamp();

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "May not rejoin search");

				return false; // in "COOLDOWN" mode
			}

			if (context.fuelTimeOnRejoin < context.minRejoinFuelTime)
			{
				this->spawnTimer.ClearStartTimestamp();

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Insufficient fuel to rejoin");

				return false; // insufficient fuel
			}

			// Confirm rejoin scheduling
			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogFull(this->pursuit, logTag, "Rejoining in", *rejoinDelay);

				if (hasLimitedFuel)
					Globals::LogPlain("Rejoining for", context.fuelTimeOnRejoin);
			}

			return true;
		}


	public: // members

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


	public: // methods

		explicit HelicopterManager(const address pursuit) : PursuitFeatures::Reaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('+', this, this->name);
		}


		~HelicopterManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('-', this, this->name);

			if (this->HasRejoinContext())
				this->rejoinContext.reset();
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
			this->VerifyPursuit   ();
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

			this->spawnTimer.ClearStartTimestamp();

			if (this->IsHelicopterRejoining())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, "Helicopter rejoined");

				this->SetFuelTime(this->rejoinContext->fuelTimeOnRejoin);

				--(this->numHelisDeployed);
			}
			else this->ProcessNewHelicopter(copVehicle);
			
			this->UpdateHelicopterStatus(Status::ACTIVE);
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (copLabel != CopLabel::HELICOPTER) return;

			Status newStatus = Status::LOST;

			if (not Globals::IsVehicleDestroyed(copVehicle))
			{
				const float* const fuelTime = this->GetFuelTimePointer();

				if ((not fuelTime) or (*fuelTime <= 0.f))
					newStatus = Status::EXPIRED;

				else if (this->ScheduleRejoining(*fuelTime))
					newStatus = Status::REJOINING;
			}
			else newStatus = Status::WRECKED;

			this->UpdateHelicopterStatus(newStatus);
		}


		[[nodiscard]] static const char* GetHelicopterName()
		{
			if (not HelicopterManager::isEnabled) return nullptr;

			if (HelicopterManager::rejoinContext)
			{
				if (HelicopterManager::rejoinContext->helicopterName)
					return HelicopterManager::rejoinContext->helicopterName;

				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid name pointer");

				ASSERT_UNREACHABLE;
			}

			return helicopterVehicle.current;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool __fastcall IsSearchSpawnAllowed(const address pursuit)
	{
		const float* const spawnChance = AsPointer<float>(Globals::GetFromPursuitLevel(pursuit, "SearchModeHeliSpawnChance"_vlt));
		
		if (not spawnChance)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Invalid SearchModeHeliSpawnChance pointer in", pursuit);

			ASSERT_UNREACHABLE_THEN(return false);
		}

		const bool isAllowed = Globals::pRNG.DoPercentTrial<float>(*spawnChance);

		if constexpr (Globals::loggingEnabled)
			Globals::LogFull(pursuit, logTag, "Search", (isAllowed) ? "allowed" : "blocked");

		return isAllowed;
	}



	[[nodiscard]] float __fastcall GetSpawnDistance(const address pursuit)
	{
		const bool  isSearch = Globals::IsPursuitInCooldownMode(pursuit);
		const auto& interval = (isSearch) ? searchSpawnDistance : chaseSpawnDistance;

		const float distance = interval.GetRandomValue();

		if constexpr (Globals::loggingEnabled)
			Globals::LogPlain("Spawn distance:", distance, (isSearch) ? "(search)" : "(chase)");

		return distance;
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Updates the amount of fuel remaining for the helicopter
	ASSEMBLY_DETOUR(FuelUpdate, /* begin = */ 0x423519, /* end = */ 0x423523)
	{
		__asm
		{
			cmp byte ptr [hasLimitedFuel], 1
			jne conclusion // unlimited fuel

			// Execute original code and resume
			fsub dword ptr [esp + 0x8]
			fst dword ptr [esi + 0x7D8]

			conclusion:
			EXIT_ASSEMBLY_DETOUR(FuelUpdate)
		}
	}



	// Sets the default helicopter fuel
	ASSEMBLY_DETOUR(DefaultFuel, 0x42AD5E, 0x42ADA1)
	{
		__asm
		{
			mov dword ptr [esi + 0x34], 0x7F7FFFFF // MAX_FLOAT

			EXIT_ASSEMBLY_DETOUR(DefaultFuel)
		}
	}



	// Checks whether the helicopter may spawn to search
	ASSEMBLY_DETOUR(SearchCheck, 0x444987, 0x44498E)
	{
		__asm
		{
			push edx

			call IsSearchSpawnAllowed // ecx: pursuit
			mov byte ptr [esi + 0x114], al

			pop edx

			lea ecx, dword ptr [esi + 0x40]

			EXIT_ASSEMBLY_DETOUR(SearchCheck)
		}
	}



	// Decides whether to request a bailout announcement
	ASSEMBLY_DETOUR(EarlyBailout, 0x717C00, 0x717C06)
	{
		static constexpr address disabledExit = 0x717C34;

		__asm
		{
			cmp byte ptr [skipBailoutSpeech], 1
			je disabled // announcement disabled

			// Execute original code and resume
			sub esp, 0x8
			push esi
			mov esi, ecx

			EXIT_ASSEMBLY_DETOUR(EarlyBailout)

			disabled:
			jmp dword ptr [disabledExit]
		}
	}



	// Sets the spawn distance to the pursuit target
	ASSEMBLY_DETOUR(SpawnDistance, 0x426ABF, 0x426AC4)
	{
		__asm
		{
			mov ecx, ebp
			call GetSpawnDistance // ecx: pursuit

			push eax
			fstp dword ptr [esp]

			EXIT_ASSEMBLY_DETOUR(SpawnDistance)
		}
	}



	// Controls whether roadblocks affect helicopter behaviour
	ASSEMBLY_DETOUR(RoadblockCheck, 0x419160, 0x419168)
	{
		__asm
		{
			cmp byte ptr [affectedByRoadblock.current], 0
			je conclusion // helicopter unaffected

			// Execute original code and resume
			mov eax, dword ptr [ecx + 0xCC]
			test eax, eax

			conclusion:
			EXIT_ASSEMBLY_DETOUR(RoadblockCheck)
		}
	}



	// Sets the cooldown for HeliStrategy 2 ramming attempts
	ASSEMBLY_DETOUR(RammingCooldown, 0x4128B2, 0x4128B9)
	{
		__asm
		{
			mov ecx, offset rammingCooldown
			call HeatParameters::Interval<float>::GetRandomValue
			fstp dword ptr [esi + 0x64] // HeliStrategy 2 cooldown

			EXIT_ASSEMBLY_DETOUR(RammingCooldown)
		}
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::fileHelicopter);

		// Heat parameters
		HeatParameters::Extract(parser, "Helicopter:Vehicle", helicopterVehicle);

		HeatParameters::Extract(parser, "Helicopter:FirstSpawn", firstSpawnDelay);

		HeatParameters::Extract(parser, "Helicopter:FuelRespawn", fuelRespawnDelay);

		HeatParameters::Extract(parser, "Helicopter:WreckRespawn", wreckRespawnDelay);

		HeatParameters::Extract(parser, "Helicopter:LostRespawn", lostRespawnDelay);

		HeatParameters::Extract(parser, "Helicopter:LostRejoin", lostRejoinDelay, minRejoinFuelTime);

		HeatParameters::Extract(parser, "Helicopter:FuelTime", fuelTime);

		HeatParameters::Extract(parser, "Helicopter:Chasing", chaseSpawnDistance);

		HeatParameters::Extract(parser, "Helicopter:Searching", searchSpawnDistance);

		HeatParameters::Extract(parser, "Helicopter:Roadblocks", affectedByRoadblock);

		HeatParameters::Extract(parser, "Helicopter:Ramming", rammingCooldown);

		// Check and make vehicle names persistent
		if (HeatParameters::ResolveHelicopterNames(helicopterVehicle))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("All vehicles valid");
		}

		// Code modifications 
		MemoryTools::Write<float*>(&maxBailoutFuelTime, {0x709F9F, 0x7078B0});

		PATCH_ASSEMBLY_DETOUR(FuelUpdate);
		PATCH_ASSEMBLY_DETOUR(DefaultFuel);
		PATCH_ASSEMBLY_DETOUR(SearchCheck);
		PATCH_ASSEMBLY_DETOUR(EarlyBailout);
		PATCH_ASSEMBLY_DETOUR(SpawnDistance);
		PATCH_ASSEMBLY_DETOUR(RoadblockCheck);
		PATCH_ASSEMBLY_DETOUR(RammingCooldown);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		helicopterVehicle.SetToHeatState(state);
		
		firstSpawnDelay.SetToHeatState(state);

		fuelRespawnDelay.SetToHeatState(state);

		wreckRespawnDelay.SetToHeatState(state);

		lostRespawnDelay.SetToHeatState(state);

		lostRejoinDelay  .SetToHeatState(state);
		minRejoinFuelTime.SetToHeatState(state);

		fuelTime.SetToHeatState(state);

		chaseSpawnDistance.SetToHeatState(state);

		searchSpawnDistance.SetToHeatState(state);

		affectedByRoadblock.SetToHeatState(state);

		rammingCooldown.SetToHeatState(state);
	}
}