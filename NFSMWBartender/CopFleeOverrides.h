#pragma once

#include <functional>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"

#include "CopSpawnTables.h"
#include "PursuitFeatures.h"



namespace CopFleeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	constinit HeatParameters::OptionalInterval<float> chaserFleeDelays({1.f}); // seconds
	constinit HeatParameters::OptionalPair    <int>   chaserThresholds({0});   // cars

	constinit HeatParameters::OptionalInterval<float> joinedRoadblockFleeDelays({1.f}); // seconds
	constinit HeatParameters::OptionalPair    <int>   joinedRoadblockThresholds({0});   // cars

	constinit HeatParameters::OptionalInterval<float> joinedHeavy3FleeDelays({1.f}); // seconds
	constinit HeatParameters::OptionalPair    <int>   joinedHeavy3Thresholds({0});   // cars

	constinit HeatParameters::Pair<float> heavy3SpeedThresholds(25.f, {0.f}); // kph
	constinit HeatParameters::Pair<bool>  heavy3JoiningEnableds(false);

	constinit HeatParameters::OptionalPair<int> heavy3JoinLimits({0}); // cars

	// Conversions
	float baseSpeedThreshold = heavy3SpeedThresholds.current / 3.6f; // mps
	float jerkSpeedThreshold = baseSpeedThreshold * .625f;           // mps





	// MembershipManager helpers --------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		// SchedulerBase class
		class SchedulerBase
		{
		protected:

			size_t numPendingExpired = 0;

			const address          pursuit;
			const std::string_view label;

			ModContainers::AddressMap<float> vehicleToTimestamp;

			// Whether currently scheduled cops should be checked for expiration
			std::function<bool ()> IsExpirationEnabled = []() -> bool {return true;};

			// Whether a given expired cop should actually be forced to bail the pursuit
			std::function<bool (const address)> ShouldBailExpired = [](const address copVehicle) -> bool {return true;};


			explicit SchedulerBase
			(
				const address          pursuit,
				const std::string_view label
			)
				: pursuit(pursuit), label(label)
			{
			}


			explicit SchedulerBase(const SchedulerBase&)   = delete;
			SchedulerBase& operator=(const SchedulerBase&) = delete;

			explicit SchedulerBase(SchedulerBase&&)   = delete;
			SchedulerBase& operator=(SchedulerBase&&) = delete;


			void ScheduleVehicle
			(
				const address copVehicle,
				const float   fleeTimer
			) {
				const auto [pair, wasAdded] = this->vehicleToTimestamp.try_emplace(copVehicle, Globals::simulationTime + fleeTimer);

				if constexpr (Globals::loggingEnabled)
				{
					if (wasAdded)
						Globals::logger.Log(this->pursuit, "[FLE]", copVehicle, "expires in", fleeTimer);

					else
						Globals::logger.Log("WARNING: [FLE]", copVehicle, "already scheduled");
				}
			}


			bool MakeVehicleBail(const address copVehicle) const
			{
				const address copAIVehiclePursuit = Globals::GetAIVehiclePursuit(copVehicle);
				if (not copAIVehiclePursuit) return false; // should never happen

				const auto StartFlee = reinterpret_cast<void(__thiscall*)(address)>(0x423370);

				StartFlee(copAIVehiclePursuit);

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE]", this->label, copVehicle, "fleeing");

				return true;
			}



		public:

			void CheckTimestamps()
			{
				auto iterator = this->vehicleToTimestamp.begin();

				while ((iterator != this->vehicleToTimestamp.end()) and this->IsExpirationEnabled())
				{
					if (Globals::simulationTime >= iterator->second)
					{
						const address copVehicle = iterator->first;

						if (this->ShouldBailExpired(copVehicle))		
							this->MakeVehicleBail(copVehicle);
						
						iterator = this->vehicleToTimestamp.erase(iterator);
					}
					else ++iterator;
				}
			}


			void ForceTriggerExpiration()
			{
				for (const auto& [copVehicle, timestamp] : this->vehicleToTimestamp)
				{
					if (this->ShouldBailExpired(copVehicle)) 
						this->MakeVehicleBail(copVehicle);

					++(this->numPendingExpired);
				}

				this->vehicleToTimestamp.clear();
				this->numPendingExpired = 0;
			}


			size_t GetNumScheduled() const
			{
				return this->vehicleToTimestamp.size() - this->numPendingExpired;
			}
		};



		// StrategyScheduler class for Strategy cops
		class StrategyScheduler : public SchedulerBase
		{
		private:

			const volatile address& strategy;



		public:

			using SchedulerBase::ShouldBailExpired;


			explicit StrategyScheduler
			(
				const address          pursuit,
				const std::string_view label,
				const int              strategyOffset
			)
				: SchedulerBase(pursuit, label), strategy(*reinterpret_cast<volatile address*>(pursuit + strategyOffset))
			{
			}


			void ReserveVehicleCapacity(const size_t numVehicles)
			{
				this->vehicleToTimestamp.reserve(numVehicles);
			}


			void AddVehicle(const address copVehicle)
			{
				const float strategyDuration = (this->strategy) ? *reinterpret_cast<volatile float*>(this->strategy + 0x8) : 1.f;

				this->ScheduleVehicle(copVehicle, strategyDuration);
			}


			void RemoveVehicle(const address copVehicle)
			{
				this->vehicleToTimestamp.erase(copVehicle);
			}


			address GetStrategy() const
			{
				return this->strategy;
			}
		};



		// PursuitScheduler class for non-Strategy cops
		class PursuitScheduler : public SchedulerBase
		{
		public:

			// Total number of active "Chaser" cops
			std::function<size_t ()> GetNumActiveChasers = []() -> size_t {return 8;};

			// Whether a given cop may be scheduled for expiration
			std::function<bool (const address)> IsSchedulable = [](const address copVehicle) -> bool {return true;};



		private:

			// Contains all cops in case of Heat transitions
			ModContainers::AddressSet vehicles;

			const HeatParameters::OptionalInterval<float>& fleeDelays;
			const HeatParameters::OptionalPair    <int>&   thresholds;


			void ReviewVehicle(const address copVehicle)
			{
				if (not Globals::playerHeatLevelKnown)       return;
				if (not this->IsSchedulable(copVehicle))     return;
				if (not this->fleeDelays.isEnableds.current) return;

				this->ScheduleVehicle(copVehicle, this->fleeDelays.GetRandomValue());
			}



		public:

			explicit PursuitScheduler
			(
				const address                                  pursuit,
				const std::string_view                         label,
				const HeatParameters::OptionalInterval<float>& fleeDelays,
				const HeatParameters::OptionalPair    <int>&   thresholds
			)
				: SchedulerBase(pursuit, label), fleeDelays(fleeDelays), thresholds(thresholds)
			{
				// Scheduled non-Strategy cops may only expire if the number of "Chaser" cops is above some threshold
				this->IsExpirationEnabled = [this]()
				{
					if (not this->thresholds.isEnableds.current) return true;

					return (static_cast<int>(this->GetNumActiveChasers()) > this->thresholds.values.current);
				};

				// Expired pursuit cops always bail and must also be removed from the extra list
				this->ShouldBailExpired = [this](const address copVehicle)
				{
					this->vehicles.erase(copVehicle);

					return true;
				};
			}


			void ReserveVehicleCapacity(const size_t numVehicles)
			{
				this->vehicles          .reserve(numVehicles);
				this->vehicleToTimestamp.reserve(numVehicles);
			}


			void ReviewAllVehicles()
			{
				this->vehicleToTimestamp.clear();

				for (const address copVehicle : this->vehicles)
					this->ReviewVehicle(copVehicle);
			}


			void AddVehicle(const address copVehicle)
			{
				this->vehicles.insert(copVehicle);
				this->ReviewVehicle(copVehicle);
			}


			void RemoveVehicle(const address copVehicle)
			{
				this->vehicles          .erase(copVehicle);
				this->vehicleToTimestamp.erase(copVehicle);
			}


			address GetNumVehicles() const
			{
				return this->vehicles.size();
			}
		};

	}





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::PursuitReaction
	{
	private:

		// Internal aliases
		using StrategyScheduler = Details::StrategyScheduler;
		using PursuitScheduler  = Details::PursuitScheduler;



	private:

		bool pursuitTargetKnown = false;

		StrategyScheduler heavyVehicles {this->pursuit, "Heavy",  0x194};
		StrategyScheduler leaderVehicles{this->pursuit, "Leader", 0x198};

		PursuitScheduler chaserVehicles         {this->pursuit, "Chaser",    chaserFleeDelays,          chaserThresholds};
		PursuitScheduler joinedHeavyVehicles    {this->pursuit, "Joined H3", joinedHeavy3FleeDelays,    joinedHeavy3Thresholds};
		PursuitScheduler joinedRoadblockVehicles{this->pursuit, "Joined RB", joinedRoadblockFleeDelays, joinedRoadblockThresholds};

		const volatile bool&    isJerk         = *reinterpret_cast<volatile bool*>   (this->pursuit + 0x238);
		const volatile address& pursuitTarget  = *reinterpret_cast<volatile address*>(this->pursuit + 0x74);


		static bool IsNotInChasersTable(const address copVehicle)
		{
			return (not CopSpawnTables::chaserSpawnTables.current->ContainsType(Globals::GetVehicleType(copVehicle)));
		}


		bool MayAnotherHeavyJoin() const
		{
			if (not heavy3JoiningEnableds.current)       return false;
			if (not heavy3JoinLimits.isEnableds.current) return true;

			return (static_cast<int>(this->joinedHeavyVehicles.GetNumVehicles()) < heavy3JoinLimits.values.current);
		}


		bool MakeHeavyVehicleJoin(const address copVehicle)
		{
			if (this->MayAnotherHeavyJoin() and Globals::EndSupportGoal(copVehicle))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE] Heavy", copVehicle, "joined");

				this->joinedHeavyVehicles.AddVehicle(copVehicle);

				return true;
			}

			return false;
		}


		address GetRigidBodyOfTarget() const
		{
			address rigidBodyOfTarget = 0x0;

			if (this->pursuitTargetKnown)
			{
				const address physicsObject = *reinterpret_cast<volatile address*>(this->pursuitTarget + 0x1C);
				
				if (physicsObject)
					rigidBodyOfTarget = *reinterpret_cast<volatile address*>(physicsObject + 0x4C);

				else if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Invalid PhysicsObject for", this->pursuitTarget, "in", this->pursuit);
			}

			return rigidBodyOfTarget;
		}


		bool IsSpeedOfTargetBelowThreshold() const
		{
			const address rigidBodyOfTarget = this->GetRigidBodyOfTarget();

			if (rigidBodyOfTarget)
			{
				const auto GetSpeedXZ = reinterpret_cast<float (__thiscall*)(address)>(0x6711F0);

				return (GetSpeedXZ(rigidBodyOfTarget) < ((this->isJerk) ? jerkSpeedThreshold : baseSpeedThreshold));
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Invalid RigidBody for", this->pursuitTarget, "in", this->pursuit);

			return false; // should never happen
		}


		void CheckForHeavyCancellation()
		{
			if (this->heavyVehicles.GetNumScheduled() == 0) return;

			if (Globals::IsInCooldownMode(this->pursuit) or this->IsSpeedOfTargetBelowThreshold())
			{
				const address heavyStrategy = this->heavyVehicles.GetStrategy();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE] Cancelling HeavyStrategy3");

				this->heavyVehicles.ForceTriggerExpiration();

				if (heavyStrategy)
				{
					const int strategyID = *reinterpret_cast<volatile int*>(heavyStrategy);

					if (strategyID == 3) // ramming SUVs
						Globals::ClearSupportRequest(this->pursuit);
				}
			}
		}



	public:

		explicit MembershipManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "MembershipManager");

			// Expired Heavy3 vehicles only bail if they cannot join as pursuit cops
			this->heavyVehicles.ShouldBailExpired = [this](const address copVehicle) -> bool
			{
				return (not this->MakeHeavyVehicleJoin(copVehicle));
			};

			// Scheduled non-Strategy cops may only expire if the number of "Chaser" cops is above some threshold
			const auto GetNumActiveChasers = [this]() -> size_t {return this->chaserVehicles.GetNumVehicles();};

			this->chaserVehicles         .GetNumActiveChasers = GetNumActiveChasers;
			this->joinedHeavyVehicles    .GetNumActiveChasers = GetNumActiveChasers;
			this->joinedRoadblockVehicles.GetNumActiveChasers = GetNumActiveChasers;

			// "Chaser" cops may only be scheduled for expiration if they aren't in the current spawn table
			this->chaserVehicles.IsSchedulable = MembershipManager::IsNotInChasersTable;

			// Memory pre-allocations
			this->heavyVehicles .ReserveVehicleCapacity(10);
			this->leaderVehicles.ReserveVehicleCapacity(10);

			this->chaserVehicles         .ReserveVehicleCapacity(50);
			this->joinedHeavyVehicles    .ReserveVehicleCapacity(20);
			this->joinedRoadblockVehicles.ReserveVehicleCapacity(20);
		}


		~MembershipManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "MembershipManager");
		}


		void UpdateOnGameplay() override 
		{
			this->CheckForHeavyCancellation();

			this->heavyVehicles .CheckTimestamps();
			this->leaderVehicles.CheckTimestamps();

			this->chaserVehicles         .CheckTimestamps();
			this->joinedHeavyVehicles    .CheckTimestamps();
			this->joinedRoadblockVehicles.CheckTimestamps();
		}


		void UpdateOnHeatChange() override 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing all vehicles");

			this->chaserVehicles         .ReviewAllVehicles();
			this->joinedHeavyVehicles    .ReviewAllVehicles();
			this->joinedRoadblockVehicles.ReviewAllVehicles();
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
			case CopLabel::CHASER:
				this->chaserVehicles.AddVehicle(copVehicle);
				return;

			case CopLabel::HEAVY:
				this->heavyVehicles.AddVehicle(copVehicle);
				return;

			case CopLabel::LEADER:
				this->leaderVehicles.AddVehicle(copVehicle);
				return;

			case CopLabel::ROADBLOCK:
				this->joinedRoadblockVehicles.AddVehicle(copVehicle);
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
			case CopLabel::CHASER:
				this->chaserVehicles.RemoveVehicle(copVehicle);
				return;

			case CopLabel::HEAVY:
				this->heavyVehicles      .RemoveVehicle(copVehicle);
				this->joinedHeavyVehicles.RemoveVehicle(copVehicle);
				return;

			case CopLabel::LEADER:
				this->leaderVehicles.RemoveVehicle(copVehicle);
				return;

			case CopLabel::ROADBLOCK:
				this->joinedRoadblockVehicles.RemoveVehicle(copVehicle);
			}
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address goalUpdateEntrance = 0x443917;
	constexpr address goalUpdateExit     = 0x44391C;

	// Prevents formations from overriding flee goals
	__declspec(naked) void GoalUpdate()
	{
		__asm
		{
			mov edi, dword ptr [ebp]
			test edi, edi
			je conclusion // invalid AIVehiclePursuit

			cmp dword ptr [edi - 0x758 + 0xC4], 0x88C018A9 // AIGoalFleePursuit

			conclusion:
			jmp dword ptr goalUpdateExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [FLE] CopFleeOverrides");

		// Heat parameters (first file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "CarSpawns.ini");

		HeatParameters::Parse(parser, "Chasers:Fleeing", chaserFleeDelays, chaserThresholds);

		// Heat parameters (second file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

		HeatParameters::Parse(parser, "Joining:Fleeing", joinedRoadblockFleeDelays, joinedRoadblockThresholds);

		// Heat parameters (third file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "Strategies.ini");

		HeatParameters::Parse(parser, "Heavy3:Cancellation", heavy3SpeedThresholds);
		HeatParameters::Parse(parser, "Heavy3:Joining",      heavy3JoiningEnableds);
		HeatParameters::Parse(parser, "Joining:Limit",       heavy3JoinLimits);
		HeatParameters::Parse(parser, "Joining:Fleeing",     joinedHeavy3FleeDelays, joinedHeavy3Thresholds);

		// Code modifications 
		MemoryTools::MakeRangeJMP<goalUpdateEntrance, goalUpdateExit>(GoalUpdate);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		if (not featureEnabled) return;

		Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");

		chaserFleeDelays.Log("chaserFleeDelay         ");
		chaserThresholds.Log("chaserThreshold         ");

		joinedRoadblockFleeDelays.Log("joinedRoadblockFleeDelay");
		joinedRoadblockThresholds.Log("joinedRoadblockThreshold");

		heavy3SpeedThresholds.Log("heay3SpeedThreshold     ");
		heavy3JoiningEnableds.Log("heavy3JoiningEnabled    ");

		if (heavy3JoiningEnableds.current)
			heavy3JoinLimits.Log("heavy3JoinLimit         ");

		joinedHeavy3FleeDelays.Log("joinedH3FleeDelay       ");
		joinedHeavy3Thresholds.Log("joinedHeavy3Threshold   ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		chaserFleeDelays.SetToHeat(isRacing, heatLevel);
		chaserThresholds.SetToHeat(isRacing, heatLevel);

		joinedRoadblockFleeDelays.SetToHeat(isRacing, heatLevel);
		joinedRoadblockThresholds.SetToHeat(isRacing, heatLevel);

		heavy3SpeedThresholds .SetToHeat(isRacing, heatLevel);
		heavy3JoiningEnableds .SetToHeat(isRacing, heatLevel);
		heavy3JoinLimits      .SetToHeat(isRacing, heatLevel);
		joinedHeavy3FleeDelays.SetToHeat(isRacing, heatLevel);
		joinedHeavy3Thresholds.SetToHeat(isRacing, heatLevel);

		baseSpeedThreshold = heavy3SpeedThresholds.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}