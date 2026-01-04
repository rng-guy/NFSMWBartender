#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "CopSpawnTables.h"
#include "PursuitFeatures.h"



namespace CopFleeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair        <float>heavy3SpeedThresholds(25.f);  // kph
	HeatParameters::Pair        <bool> heavy3JoiningEnableds(false);
	HeatParameters::OptionalPair<int>  heavy3JoinLimits;             // cars

	HeatParameters::OptionalInterval<float> chaserFleeDelays;           // seconds
	HeatParameters::Pair            <int>   chaserChasersThresholds(2); // cars

	HeatParameters::OptionalInterval<float> joinedFleeDelays;           // seconds
	HeatParameters::Pair            <int>   joinedChasersThresholds(2); // cars

	// Conversions
	float baseSpeedThreshold = heavy3SpeedThresholds.current / 3.6f; // mps
	float jerkSpeedThreshold = baseSpeedThreshold * .625f;           // mps





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::PursuitReaction
	{
	private:

		bool pursuitTargetKnown = false;

		HashContainers::AddressSet chaserVehicles;
		HashContainers::AddressSet joinedRBVehicles;
		HashContainers::AddressSet joinedHeavy3Vehicles;

		HashContainers::AddressMap<float> chaserVehicleToTimestamp;
		HashContainers::AddressMap<float> joinedRBVehicleToTimestamp;

		HashContainers::AddressMap<float> heavy3VehicleToTimestamp;
		HashContainers::AddressMap<float> leaderVehicleToTimestamp;

		const bool&    isJerk         = *reinterpret_cast<bool*>   (this->pursuit + 0x238);
		const address& heavyStrategy  = *reinterpret_cast<address*>(this->pursuit + 0x194);
		const address& leaderStrategy = *reinterpret_cast<address*>(this->pursuit + 0x198);
		const address& pursuitTarget  = *reinterpret_cast<address*>(this->pursuit + 0x74);

		inline static HashContainers::AddressMap<MembershipManager*> pursuitToManager;


		void ScheduleVehicle
		(
			const address                      copVehicle,
			const float                        vehicleFleeDelay,
			HashContainers::AddressMap<float>& vehicleToTimestamp
		) 
			const 
		{
			const float fleeTimestamp    = Globals::simulationTime + vehicleFleeDelay;
			const auto  [pair, wasAdded] = vehicleToTimestamp.try_emplace(copVehicle, fleeTimestamp);

			if constexpr (Globals::loggingEnabled)
			{
				if (wasAdded)
					Globals::logger.Log(this->pursuit, "[FLE]", copVehicle, "expires in", vehicleFleeDelay);

				else
					Globals::logger.Log("WARNING: [FLE]", copVehicle, "already scheduled");
			}
		}


		void ScheduleStrategyVehicle
		(
			const address                      copVehicle,
			const address                      strategy,
			HashContainers::AddressMap<float>& vehicleToTimestamp
		) 
			const
		{
			if (not Globals::playerHeatLevelKnown) return;

			const float strategyDuration = (strategy) ? *reinterpret_cast<float*>(strategy + 0x8) : 1.f;
			this->ScheduleVehicle(copVehicle, strategyDuration, vehicleToTimestamp);
		}


		void ReviewChaserVehicle(const address copVehicle)
		{
			if (not Globals::playerHeatLevelKnown)       return;
			if (not chaserFleeDelays.isEnableds.current) return;

			const vault copType  = Globals::GetVehicleType(copVehicle);
			const bool  isMember = CopSpawnTables::chaserSpawnTables.current->ContainsType(copType);

			if (not isMember)
				this->ScheduleVehicle(copVehicle, chaserFleeDelays.GetRandomValue(), this->chaserVehicleToTimestamp);
		}


		void ReviewAllChaserVehicles()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing chaser vehicles");

			this->chaserVehicleToTimestamp.clear();

			for (const address copVehicle : this->chaserVehicles)
				this->ReviewChaserVehicle(copVehicle);
		}


		void ReviewJoinedRBVehicle(const address copVehicle)
		{
			if (not Globals::playerHeatLevelKnown)       return;
			if (not joinedFleeDelays.isEnableds.current) return;

			this->ScheduleVehicle(copVehicle, joinedFleeDelays.GetRandomValue(), this->joinedRBVehicleToTimestamp);
		}


		void ReviewAllJoinedRBVehicles()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing joined RB vehicles");

			this->joinedRBVehicleToTimestamp.clear();

			for (const address copVehicle : this->joinedRBVehicles)
				this->ReviewJoinedRBVehicle(copVehicle);
		}


		bool MayAnotherVolatileFlee(const int chasersThreshold) const
		{
			return (static_cast<int>(this->chaserVehicles.size()) > chasersThreshold);
		}


		static bool MakeVehicleBail(const address copVehicle)
		{
			const address pursuitVehicle = PursuitReaction::GetPursuitVehicle(copVehicle);

			static const auto StartFlee = reinterpret_cast<void(__thiscall*)(address)>(0x423370);

			if (pursuitVehicle)
				StartFlee(pursuitVehicle);

			return pursuitVehicle;
		}


		void CheckVolatileVehicles
		(
			HashContainers::AddressSet&        vehicles,
			HashContainers::AddressMap<float>& vehicleToTimestamp,
			const int                          chasersThreshold
		) 
			const
		{
			if (vehicleToTimestamp.empty())                         return;
			if (not this->MayAnotherVolatileFlee(chasersThreshold)) return;

			auto iterator = vehicleToTimestamp.begin();

			while (iterator != vehicleToTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeVehicleBail(iterator->first);
					vehicles.erase(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Volatile vehicle", iterator->first, "expired");

					iterator = vehicleToTimestamp.erase(iterator);

					if (not this->MayAnotherVolatileFlee(chasersThreshold))
					{
						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[FLE] Fleeing suspended");

						break;
					}
				}
				else ++iterator;
			}
		}


		bool MayAnotherHeavyJoin() const
		{
			if (not heavy3JoiningEnableds.current)       return false;
			if (not heavy3JoinLimits.isEnableds.current) return true;

			return (static_cast<int>(this->joinedHeavy3Vehicles.size()) < heavy3JoinLimits.values.current);
		}


		void MakeHeavy3VehicleBail(const address copVehicle)
		{
			if (this->MayAnotherHeavyJoin() and this->EndSupportGoal(copVehicle))
			{
				this->joinedHeavy3Vehicles.insert(copVehicle);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE] Heavy3 vehicle", copVehicle, "joined");
			}
			else
			{
				this->MakeVehicleBail(copVehicle);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE] Heavy3 vehicle", copVehicle, "expired");
			}
		}


		void CheckHeavy3Vehicles()
		{
			if (this->heavy3VehicleToTimestamp.empty()) return;

			auto iterator = this->heavy3VehicleToTimestamp.begin();

			while (iterator != this->heavy3VehicleToTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeHeavy3VehicleBail(iterator->first);

					iterator = this->heavy3VehicleToTimestamp.erase(iterator);
				}
				else ++iterator;
			}
		}


		void CheckLeaderVehicles()
		{
			if (this->leaderVehicleToTimestamp.empty()) return;

			auto iterator = this->leaderVehicleToTimestamp.begin();

			while (iterator != this->leaderVehicleToTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeVehicleBail(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Leader vehicle", iterator->first, "expired");

					iterator = this->leaderVehicleToTimestamp.erase(iterator);
				}
				else ++iterator;
			}
		}


		address GetRigidBodyOfTarget() const
		{
			if (this->pursuitTargetKnown)
			{
				const address physicsObject     = *reinterpret_cast<address*>(this->pursuitTarget + 0x1C);
				const address rigidBodyOfTarget = (physicsObject) ? *reinterpret_cast<address*>(physicsObject + 0x4C) : 0x0;

				if constexpr (Globals::loggingEnabled)
				{
					if (not rigidBodyOfTarget)
						Globals::logger.Log("WARNING: [FLE] Invalid PhysicsObject for target in", this->pursuit);
				}

				return rigidBodyOfTarget;
			}

			return 0x0;
		}


		bool IsSpeedOfTargetBelowThreshold() const
		{
			const address rigidBodyOfTarget = this->GetRigidBodyOfTarget();

			if (rigidBodyOfTarget)
			{
				static const auto GetSpeedXZ = reinterpret_cast<float (__thiscall*)(address)>(0x6711F0);

				return (GetSpeedXZ(rigidBodyOfTarget) < ((this->isJerk) ? jerkSpeedThreshold : baseSpeedThreshold));
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Invalid RigidBody for target", this->pursuitTarget, "in", this->pursuit);

			return false; // should never happen
		}


		void CheckForHeavy3Cancellation()
		{
			if (this->heavy3VehicleToTimestamp.empty()) return;

			if (this->IsSearchModeActive() or this->IsSpeedOfTargetBelowThreshold())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE] Cancelling HeavyStrategy3");

				for (const auto& [copVehicle, fleeTimestamp] : this->heavy3VehicleToTimestamp)
					this->MakeHeavy3VehicleBail(copVehicle);

				this->heavy3VehicleToTimestamp.clear();

				if (this->heavyStrategy)
					Globals::ClearSupportRequest(this->pursuit);
			}
		}



	public:

		explicit MembershipManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "MembershipManager");
		}


		~MembershipManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "MembershipManager");
		}


		void UpdateOnGameplay() override 
		{
			this->CheckHeavy3Vehicles();
			this->CheckLeaderVehicles();

			this->CheckForHeavy3Cancellation();

			this->CheckVolatileVehicles(this->chaserVehicles,   this->chaserVehicleToTimestamp,   chaserChasersThresholds.current);
			this->CheckVolatileVehicles(this->joinedRBVehicles, this->joinedRBVehicleToTimestamp, joinedChasersThresholds.current);
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewAllChaserVehicles();
			this->ReviewAllJoinedRBVehicles();
		}


		void UpdateOncePerPursuit() override
		{
			this->pursuitTargetKnown = true;
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				this->ScheduleStrategyVehicle(copVehicle, this->heavyStrategy, this->heavy3VehicleToTimestamp);
				return;

			case CopLabel::LEADER:
				this->ScheduleStrategyVehicle(copVehicle, this->leaderStrategy, this->leaderVehicleToTimestamp);
				return;

			case CopLabel::CHASER:
				this->chaserVehicles.insert(copVehicle);
				this->ReviewChaserVehicle  (copVehicle);
				return;

			case CopLabel::ROADBLOCK:
				this->joinedRBVehicles.insert(copVehicle);
				this->ReviewJoinedRBVehicle  (copVehicle);
			}
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				this->joinedHeavy3Vehicles    .erase(copVehicle);
				this->heavy3VehicleToTimestamp.erase(copVehicle);
				return;

			case CopLabel::LEADER:
				this->leaderVehicleToTimestamp.erase(copVehicle);
				return;

			case CopLabel::CHASER:
				this->chaserVehicles          .erase(copVehicle);
				this->chaserVehicleToTimestamp.erase(copVehicle);
				return;

			case CopLabel::ROADBLOCK:
				this->joinedRBVehicles          .erase(copVehicle);
				this->joinedRBVehicleToTimestamp.erase(copVehicle);
			}
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address goalUpdateEntrance = 0x443917;
	constexpr address goalUpdateExit     = 0x44391C;

	__declspec(naked) void GoalUpdate()
	{
		__asm
		{
			mov edi, dword ptr [ebp]
			test edi, edi
			je conclusion // invalid vehicle

			cmp dword ptr [edi - 0x758 + 0xC4], 0x88C018A9 // AIGoalFleePursuit

			conclusion:
			jmp dword ptr goalUpdateExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		// Heat parameters I
		parser.LoadFile(HeatParameters::configPathAdvanced + "CarSpawns.ini");

		HeatParameters::ParseOptional<float, int>(parser, "Joining:Fleeing", {joinedFleeDelays, 1.f}, {joinedChasersThresholds, 0});
		HeatParameters::ParseOptional<float, int>(parser, "Chasers:Fleeing", {chaserFleeDelays, 1.f}, {chaserChasersThresholds, 0});

		// Heat parameters II
		parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini");

		HeatParameters::Parse        <float>(parser, "Heavy3:Cancellation", {heavy3SpeedThresholds, 0.f});
		HeatParameters::Parse        <bool> (parser, "Heavy3:Joining",      {heavy3JoiningEnableds});
		HeatParameters::ParseOptional<int>  (parser, "Joining:Limit",       {heavy3JoinLimits,      0});

		// Code modifications 
		MemoryTools::MakeRangeJMP(GoalUpdate, goalUpdateEntrance, goalUpdateExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		if (not featureEnabled) return;

		Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");

		heavy3SpeedThresholds.Log("heay3SpeedThreshold     ");
		heavy3JoiningEnableds.Log("heavy3JoiningEnabled    ");

		if (heavy3JoiningEnableds.current)
			heavy3JoinLimits.Log("heavy3JoinLimit         ");

		if (chaserFleeDelays.isEnableds.current)
		{
			chaserFleeDelays       .Log("chaserFleeDelay         ");
			chaserChasersThresholds.Log("chaserChasersThreshold  ");
		}

		if (joinedFleeDelays.isEnableds.current)
		{
			joinedFleeDelays       .Log("joinedFleeDelay         ");
			joinedChasersThresholds.Log("joinedChasersThreshold  ");
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		heavy3SpeedThresholds.SetToHeat(isRacing, heatLevel);
		heavy3JoiningEnableds.SetToHeat(isRacing, heatLevel);
		heavy3JoinLimits     .SetToHeat(isRacing, heatLevel);

		baseSpeedThreshold = heavy3SpeedThresholds.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;

		chaserFleeDelays       .SetToHeat(isRacing, heatLevel);
		chaserChasersThresholds.SetToHeat(isRacing, heatLevel);

		joinedFleeDelays       .SetToHeat(isRacing, heatLevel);
		joinedChasersThresholds.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}