#pragma once

#include <optional>
#include <algorithm>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/HeatParameters.hpp"

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

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, fuelTime, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, fuelRespawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, wreckRespawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, lostRespawnDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(float, catchUpThreshold, {0.f}); // metres

	constinit HEAT_PARAMETER_INTERVAL(float, chaseSpawnDistance, 250.f, 250.f, {0.f, 450.f}); // metres

	constinit HEAT_PARAMETER_INTERVAL(float, searchSpawnDistance, 250.f, 250.f, {0.f, 450.f}); // metres

	constinit HEAT_PARAMETER_VALUE(bool, affectedByRoadblock, true);

	constinit HEAT_PARAMETER_INTERVAL(float, rammingCooldown, 8.f, 8.f, {1.f}); // seconds

	// Parameter conversions
	float squaredCatchUpThreshold; // metres squared





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
			LOST
		};

		
	private: // members
		
		bool isPlayerPursuit = false;

		Status helicopterStatus = Status::PENDING;

		PursuitFeatures::IntervalTimer spawnTimer;

		bool& maySpawnToSearch = AsReference<bool>(this->pursuit + 0xD4);

		inline static constinit bool isFuelLimited = false;

		inline static constexpr Globals::LogLiteral name = "HelicopterManager";


	private: // methods

		void CheckForPlayerPursuit()
		{
			this->isPlayerPursuit = Globals::IsPlayerPursuit(this->pursuit);

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, (this->isPlayerPursuit) ? "Is" : "Not", "player pursuit");
		}


		[[nodiscard]] static float* GetFuelTimePointer()
		{ 
			return (Globals::helicopter) ? AsPointer<float>(Globals::helicopter + 0x7D8) : nullptr;
		}


		void SetFuelTime(const float amount) const
		{
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
			this->maySpawnToSearch = false;

			this->isFuelLimited = fuelTime.isEnabled.current;

			if (this->isFuelLimited)
				this->SetFuelTime(fuelTime.interval.GetRandomValue());

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Helicopter spawned");
		}


		[[nodiscard]] bool IsBlockedByCooldownMode() const
		{
			return (Globals::IsPursuitInCooldownMode(this->pursuit) and (not this->maySpawnToSearch));
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

		
		[[nodiscard]] static bool IsRoadblockSpawnPending()
		{
			return AsReference<address>(Globals::copManager + 0xBC);
		}


		void MakeSpawnAttempt() const
		{
			if (Globals::helicopter)               return;
			if (not this->isPlayerPursuit)         return;
			if (not this->spawnTimer.HasExpired()) return;
			if (this->IsBlockedByCooldownMode())   return;
			if (this->IsRoadblockSpawnPending())   return;

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
			this->CheckForPlayerPursuit();
			this->UpdateSpawnTimer     ();
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

			this->ProcessNewHelicopter(copVehicle);
			
			this->helicopterStatus = Status::ACTIVE;
			this->UpdateSpawnTimer();
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
			}
			else newStatus = Status::WRECKED;

			this->helicopterStatus = newStatus;
			this->UpdateSpawnTimer();
		}


		[[nodiscard]] static bool __cdecl IsFuelLimited()
		{
			return HelicopterManager::isFuelLimited;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool __fastcall MaySpawnToSearch(const address pursuit)
	{
		if (Globals::helicopter) return false;

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



	void __stdcall EnforceCatchUpThreshold
	(
		const float deltaX,
		const float deltaZ
	) {
		if (not catchUpThreshold.isEnabled.current) return;

		bool& ignoreHeliSheet = AsReference<bool>(0x90D621);
		if (ignoreHeliSheet) return; // no need to change

		const float squaredDistance = deltaX * deltaX + deltaZ * deltaZ;
		ignoreHeliSheet = (squaredDistance >= squaredCatchUpThreshold);
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Updates the amount of fuel remaining for the helicopter
	ASSEMBLY_DETOUR(FuelUpdate, /* begin = */ 0x423519, /* end = */ 0x423523)
	{
		__asm
		{
			call HelicopterManager::IsFuelLimited
			test al, al
			je conclusion // unlimited fuel

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

			call MaySpawnToSearch // ecx: pursuit
			mov byte ptr [esi + 0x114], al

			pop edx

			lea ecx, dword ptr [esi + 0x40]

			EXIT_ASSEMBLY_DETOUR(SearchCheck)
		}
	}



	// Attempts to spawn a new helicopter
	ASSEMBLY_DETOUR(SpawnAttempt, 0x4269D0, 0x4269E6)
	{
		__asm
		{
			push dword ptr [helicopterVehicle.current]

			mov dword ptr [esp + 0x48], esp // probably dead store

			mov ecx, dword ptr [esp + 0x38]
			call Globals::GetAvailableCopVehicleByName

			EXIT_ASSEMBLY_DETOUR(SpawnAttempt)
		}
	}



	// Sets the spawn distance to the pursuit target
	ASSEMBLY_DETOUR(SpawnDistance, 0x426ABF, 0x426AC4)
	{
		__asm
		{
			sub esp, 0x4

			mov ecx, ebp
			call GetSpawnDistance // ecx: pursuit
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



	// Calculates the helicopter's distance to its target
	ASSEMBLY_DETOUR(TargetDistance, 0x4127F9, 0x412803)
	{
		__asm
		{
			push eax

			push dword ptr [esp + 0x24] // deltaZ
			push dword ptr [esp + 0x20] // deltaX
			call EnforceCatchUpThreshold

			pop ecx

			// Execute original code and resume
			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0x11C]

			EXIT_ASSEMBLY_DETOUR(TargetDistance)
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





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	void UpdateParameterConversions()
	{
		squaredCatchUpThreshold = catchUpThreshold.value.current * catchUpThreshold.value.current;
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::fileHelicopter);

		// Heat parameters
		HeatParameters::Extract(parser, "Helicopter:Vehicle", helicopterVehicle);

		HeatParameters::Extract(parser, "Helicopter:FirstSpawn", firstSpawnDelay);

		HeatParameters::Extract(parser, "Helicopter:FuelTime", fuelTime);

		HeatParameters::Extract(parser, "Helicopter:FuelRespawn", fuelRespawnDelay);

		HeatParameters::Extract(parser, "Helicopter:WreckRespawn", wreckRespawnDelay);

		HeatParameters::Extract(parser, "Helicopter:LostRespawn", lostRespawnDelay);

		HeatParameters::Extract(parser, "Helicopter:Pathing", catchUpThreshold);

		HeatParameters::Extract(parser, "Helicopter:Chasing", chaseSpawnDistance);

		HeatParameters::Extract(parser, "Helicopter:Searching", searchSpawnDistance);

		HeatParameters::Extract(parser, "Helicopter:Roadblocks", affectedByRoadblock);

		HeatParameters::Extract(parser, "Helicopter:Ramming", rammingCooldown);

		// Parameter conversions
		UpdateParameterConversions(); // uses vanilla value(s)

		// Check and make vehicle names persistent
		if (HeatParameters::ResolveHelicopterNames(helicopterVehicle))
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("All vehicles valid");
		}

		// Code modifications 
		MemoryTools::MakeRangeNOP<0x43EBA7, 0x43EBBE>(); // helicopter spawn (first  part)
		MemoryTools::MakeRangeNOP<0x43EBC0, 0x43EBCA>(); // helicopter spawn (second part)

		PATCH_ASSEMBLY_DETOUR(FuelUpdate);
		PATCH_ASSEMBLY_DETOUR(DefaultFuel);
		PATCH_ASSEMBLY_DETOUR(SearchCheck);
		PATCH_ASSEMBLY_DETOUR(SpawnAttempt);
		PATCH_ASSEMBLY_DETOUR(SpawnDistance);
		PATCH_ASSEMBLY_DETOUR(RoadblockCheck);
		PATCH_ASSEMBLY_DETOUR(TargetDistance);
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

		// Heat parameters
		helicopterVehicle.SetToHeatState(state);
		
		firstSpawnDelay.SetToHeatState(state);

		fuelTime.SetToHeatState(state);

		fuelRespawnDelay.SetToHeatState(state);

		wreckRespawnDelay.SetToHeatState(state);

		lostRespawnDelay.SetToHeatState(state);

		catchUpThreshold.SetToHeatState(state);

		chaseSpawnDistance.SetToHeatState(state);

		searchSpawnDistance.SetToHeatState(state);

		affectedByRoadblock.SetToHeatState(state);

		rammingCooldown.SetToHeatState(state);

		// Conversions
		UpdateParameterConversions();
	}
}