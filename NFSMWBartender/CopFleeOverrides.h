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

	bool anyFeatureEnabled = false;

	// Heat parameters
	constinit HeatParameters::OptionalInterval<float> chaserFleeDelay({1.f}); // seconds
	constinit HeatParameters::OptionalValue   <int>   chaserThreshold({0});   // cars

	constinit HeatParameters::OptionalInterval<float> joinedRoadblockFleeDelay({1.f}); // seconds
	constinit HeatParameters::OptionalValue   <int>   joinedRoadblockThreshold({0});   // cars

	constinit HeatParameters::OptionalInterval<float> joinedHeavy3FleeDelay({1.f}); // seconds
	constinit HeatParameters::OptionalValue   <int>   joinedHeavy3Threshold({0});   // cars

	constinit HeatParameters::Value<float> heavy3SpeedThreshold(25.f, {0.f}); // kph
	constinit HeatParameters::Value<bool>  heavy3JoiningEnabled(false);

	constinit HeatParameters::OptionalValue<int> heavy3JoinLimit({0}); // cars

	// Conversions
	float baseSpeedThreshold = heavy3SpeedThreshold.current / 3.6f; // mps
	float jerkSpeedThreshold = baseSpeedThreshold * .625f;          // mps

	// Inline hashes for ASM
	enum class VaultHash : vault
	{
		AIGOALFLEEPURSUIT = "AIGoalFleePursuit"_vlt
	};





	// MembershipManager helpers --------------------------------------------------------------------------------------------------------------------

	namespace Details
	{

		// Base expiration-time tracker for cops
		class SchedulerBase
		{
		protected:

			size_t numPendingExpired = 0;

			const address          pursuit;
			const std::string_view vehicleLabel;

			ModContainers::AddressMap<float> copVehicleToTimestamp;

			// Whether currently scheduled cops should be checked for expiration
			std::function<bool ()> ShouldCheckForExpiration = []() -> bool {return true;};

			// Whether a given expired cop vehicle should actually be forced to bail the pursuit
			std::function<bool (const address)> ShouldExpiredVehicleBail = [](const address copVehicle) -> bool {return true;};


			explicit SchedulerBase
			(
				const address          pursuit,
				const std::string_view vehicleLabel
			)
				: pursuit(pursuit), vehicleLabel(vehicleLabel)
			{
			}


			explicit SchedulerBase(SchedulerBase&&)      = delete;
			explicit SchedulerBase(const SchedulerBase&) = delete;

			SchedulerBase& operator=(SchedulerBase&&)      = delete;
			SchedulerBase& operator=(const SchedulerBase&) = delete;


			void ScheduleVehicle
			(
				const address copVehicle,
				const float   fleeTimer
			) {
				const auto [pairIt, isNewVehicle] = this->copVehicleToTimestamp.try_emplace(copVehicle, Globals::simulationTime + fleeTimer);

				if constexpr (Globals::loggingEnabled)
				{
					if (isNewVehicle)
						Globals::logger.Log(this->pursuit, "[FLE]", copVehicle, "expires in", fleeTimer);

					else
						Globals::logger.Log("WARNING: [FLE]", copVehicle, "already scheduled");
				}
			}


			bool MakeVehicleBail(const address copVehicle) const
			{
				const address copAIVehiclePursuit = Globals::GetAIVehiclePursuit(copVehicle);
				if (not copAIVehiclePursuit) return false; // should never happen

				const auto StartFlee = AsFunction<void __thiscall (address)>(0x423370);
				StartFlee(copAIVehiclePursuit); // also updates vehicle goal(s) accordingly

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE]", this->vehicleLabel, copVehicle, "fleeing");

				return true;
			}


		public:

			void CheckTimestamps()
			{
				auto pairIt = this->copVehicleToTimestamp.begin();

				while ((pairIt != this->copVehicleToTimestamp.end()) and this->ShouldCheckForExpiration())
				{
					const auto& [copVehicle, timestamp] = *pairIt;

					if (Globals::simulationTime >= timestamp)
					{
						if (this->ShouldExpiredVehicleBail(copVehicle))
							this->MakeVehicleBail(copVehicle);
						
						pairIt = this->copVehicleToTimestamp.erase(pairIt);
					}
					else ++pairIt;
				}
			}


			void ForceTriggerExpiration()
			{
				for (const auto& [copVehicle, timestamp] : this->copVehicleToTimestamp)
				{
					if (this->ShouldExpiredVehicleBail(copVehicle))
						this->MakeVehicleBail(copVehicle);

					++(this->numPendingExpired);
				}

				this->copVehicleToTimestamp.clear();
				this->numPendingExpired = 0;
			}


			[[nodiscard]] size_t GetNumScheduled() const
			{
				return this->copVehicleToTimestamp.size() - this->numPendingExpired;
			}
		};



		// Expiration-time tracker for Strategy cops
		class StrategyScheduler : public SchedulerBase
		{
		private:

			const volatile address& strategy;


		public:

			using SchedulerBase::ShouldExpiredVehicleBail;


			explicit StrategyScheduler
			(
				const address          pursuit,
				const std::string_view vehicleLabel,
				const ptrdiff_t        strategyOffset
			)
				: SchedulerBase(pursuit, vehicleLabel), strategy(AsVolatile<address>(pursuit + strategyOffset))
			{
			}


			void ReserveVehicleCapacity(const size_t numVehicles)
			{
				this->copVehicleToTimestamp.reserve(numVehicles);
			}


			void AddVehicle(const address copVehicle)
			{
				const float strategyDuration = (this->strategy) ? AsVolatile<float>(this->strategy + 0x8) : 1.f;
				this->ScheduleVehicle(copVehicle, strategyDuration); // should never be 1.f (unless user-defined)
			}


			void RemoveVehicle(const address copVehicle)
			{
				this->copVehicleToTimestamp.erase(copVehicle);
			}


			[[nodiscard]] address GetStrategy() const
			{
				return this->strategy;
			}
		};



		// Expiration-time tracker for non-Strategy cops
		class PursuitScheduler : public SchedulerBase
		{
		public:

			// Total number of active "Chasers"
			std::function<size_t ()> GetNumActiveChasers = []() -> size_t {return 8;};

			// Whether a given cop vehicle may be scheduled for expiration
			std::function<bool (const address)> IsSchedulable = [](const address copVehicle) -> bool {return true;};


		private:

			// Tracks all cops in case of Heat transitions
			ModContainers::AddressSet copVehicles;

			const HeatParameters::OptionalInterval<float>& fleeDelay;
			const HeatParameters::OptionalValue   <int>&   fleeThreshold;


			void ReviewVehicle(const address copVehicle)
			{
				if (not Globals::playerHeatLevelKnown)     return;
				if (not this->IsSchedulable(copVehicle))   return;
				if (not this->fleeDelay.isEnabled.current) return;

				this->ScheduleVehicle(copVehicle, this->fleeDelay.GetRandomValue());
			}


		public:

			explicit PursuitScheduler
			(
				const address                                  pursuit,
				const std::string_view                         vehicleLabel,
				const HeatParameters::OptionalInterval<float>& fleeDelay,
				const HeatParameters::OptionalValue   <int>&   fleeThreshold
			)
				: SchedulerBase(pursuit, vehicleLabel), fleeDelay(fleeDelay), fleeThreshold(fleeThreshold)
			{
				// Scheduled non-Strategy cops may only expire if the number of "Chasers" is above some threshold
				this->ShouldCheckForExpiration = [this]() -> bool
				{
					if (this->fleeThreshold.isEnabled.current)
						return (static_cast<int>(this->GetNumActiveChasers()) > this->fleeThreshold.value.current);

					return true;
				};

				// Expired pursuit cops always bail and must also be un-tracked
				this->ShouldExpiredVehicleBail = [this](const address copVehicle) -> bool
				{
					return this->copVehicles.erase(copVehicle);
				};
			}


			explicit PursuitScheduler
			(
				const address, 
				const std::string_view, 
				const HeatParameters::OptionalInterval<float>&&, 
				const HeatParameters::OptionalValue   <int>&&
			) 
				= delete;


			void ReserveVehicleCapacity(const size_t numVehicles)
			{
				this->copVehicles          .reserve(numVehicles);
				this->copVehicleToTimestamp.reserve(numVehicles);
			}


			void ReviewAllVehicles()
			{
				this->copVehicleToTimestamp.clear();

				for (const address copVehicle : this->copVehicles)
					this->ReviewVehicle(copVehicle);
			}


			void AddVehicle(const address copVehicle)
			{
				this->copVehicles.insert(copVehicle);
				this->ReviewVehicle(copVehicle);
			}


			void RemoveVehicle(const address copVehicle)
			{
				this->copVehicles          .erase(copVehicle);
				this->copVehicleToTimestamp.erase(copVehicle);
			}


			[[nodiscard]] size_t GetNumVehicles() const
			{
				return this->copVehicles.size();
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

		PursuitScheduler chaserVehicles         {this->pursuit, "Chaser",    chaserFleeDelay,          chaserThreshold};
		PursuitScheduler joinedHeavyVehicles    {this->pursuit, "Joined H3", joinedHeavy3FleeDelay,    joinedHeavy3Threshold};
		PursuitScheduler joinedRoadblockVehicles{this->pursuit, "Joined RB", joinedRoadblockFleeDelay, joinedRoadblockThreshold};

		const volatile bool&    isJerk         = AsVolatile<bool>   (this->pursuit + 0x238);
		const volatile address& pursuitTarget  = AsVolatile<address>(this->pursuit + 0x74);


		[[nodiscard]] static bool IsNotInChaserTable(const address copVehicle)
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return (not CopSpawnTables::chaserSpawnTable.current->ContainsCopType(copType));
		}


		[[nodiscard]] bool MayAnotherHeavyJoin() const
		{
			if (not heavy3JoiningEnabled     .current) return false;
			if (not heavy3JoinLimit.isEnabled.current) return true;

			const int numJoinedHeavy3s = static_cast<int>(this->joinedHeavyVehicles.GetNumVehicles());

			return (numJoinedHeavy3s < heavy3JoinLimit.value.current);
		}


		bool MakeHeavyVehicleJoin(const address copVehicle)
		{
			if (not this->MayAnotherHeavyJoin())         return false;
			if (not Globals::EndSupportGoal(copVehicle)) return false;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Heavy", copVehicle, "joined");

			this->joinedHeavyVehicles.AddVehicle(copVehicle);

			return true;
		}


		[[nodiscard]] address GetRigidBodyOfTarget() const
		{
			address rigidBodyOfTarget = 0x0;

			if (this->pursuitTargetKnown)
			{
				const address physicsObject = AsVolatile<address>(this->pursuitTarget + 0x1C);
				
				if (physicsObject)
					rigidBodyOfTarget = AsVolatile<address>(physicsObject + 0x4C);

				else if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Invalid PhysicsObject for", this->pursuitTarget, "in", this->pursuit);
			}

			return rigidBodyOfTarget;
		}


		[[nodiscard]] bool IsSpeedOfTargetBelowThreshold() const
		{
			const address rigidBodyOfTarget = this->GetRigidBodyOfTarget();

			if (rigidBodyOfTarget)
			{
				const auto  GetSpeedXZ     = AsFunction<float __thiscall (address)>(0x6711F0);
				const float speedThreshold = (this->isJerk) ? jerkSpeedThreshold : baseSpeedThreshold;

				return (GetSpeedXZ(rigidBodyOfTarget) < speedThreshold);
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
					const int strategyID = AsVolatile<int>(heavyStrategy);

					if (strategyID == 3) // ramming SUVs
						Globals::ClearSupportRequest(this->pursuit);
				}
			}
		}


	public:

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


		explicit MembershipManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "MembershipManager");

			// Expired Heavy3 vehicles only bail if they cannot join as pursuit cops
			this->heavyVehicles.ShouldExpiredVehicleBail = [this](const address copVehicle) -> bool
			{
				return (not this->MakeHeavyVehicleJoin(copVehicle));
			};

			// Scheduled non-Strategy cops may only expire if the number of "Chasers" is above some threshold
			const auto GetNumActiveChasers = [this]() -> size_t {return this->chaserVehicles.GetNumVehicles();};

			this->chaserVehicles         .GetNumActiveChasers = GetNumActiveChasers;
			this->joinedHeavyVehicles    .GetNumActiveChasers = GetNumActiveChasers;
			this->joinedRoadblockVehicles.GetNumActiveChasers = GetNumActiveChasers;

			// "Chasers" may only be scheduled for expiration if they aren't in the current spawn table
			this->chaserVehicles.IsSchedulable = MembershipManager::IsNotInChaserTable;

			// Container pre-allocations
			this->heavyVehicles .ReserveVehicleCapacity(10);
			this->leaderVehicles.ReserveVehicleCapacity(10);

			this->chaserVehicles         .ReserveVehicleCapacity(50);
			this->joinedHeavyVehicles    .ReserveVehicleCapacity(10);
			this->joinedRoadblockVehicles.ReserveVehicleCapacity(10);
		}


		~MembershipManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "MembershipManager");
		}


		void ReactToGameplay() override 
		{
			this->CheckForHeavyCancellation();

			this->heavyVehicles .CheckTimestamps();
			this->leaderVehicles.CheckTimestamps();

			this->chaserVehicles         .CheckTimestamps();
			this->joinedHeavyVehicles    .CheckTimestamps();
			this->joinedRoadblockVehicles.CheckTimestamps();
		}


		void ReactToHeatStateUpdate() override 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing all vehicles");

			this->chaserVehicles         .ReviewAllVehicles();
			this->joinedHeavyVehicles    .ReviewAllVehicles();
			this->joinedRoadblockVehicles.ReviewAllVehicles();
		}


		void ReactToPursuitStartWithDelay() override
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
		using enum VaultHash;

		__asm
		{
			mov edi, dword ptr [ebp]
			test edi, edi
			je conclusion // invalid AIVehiclePursuit

			cmp dword ptr [edi - 0x758 + 0xC4], AIGOALFLEEPURSUIT

			conclusion:
			jmp dword ptr [goalUpdateExit]
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [FLE] CopFleeOverrides");

		// Heat parameters (first file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "CarSpawns.ini");

		HeatParameters::Parse(parser, "Chasers:Fleeing", chaserFleeDelay, chaserThreshold);

		// Heat parameters (second file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

		HeatParameters::Parse(parser, "Joining:Fleeing", joinedRoadblockFleeDelay, joinedRoadblockThreshold);

		// Heat parameters (third file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "Strategies.ini");

		HeatParameters::Parse(parser, "Heavy3:Cancellation", heavy3SpeedThreshold);
		HeatParameters::Parse(parser, "Heavy3:Joining",      heavy3JoiningEnabled);
		HeatParameters::Parse(parser, "Joining:Limit",       heavy3JoinLimit);
		HeatParameters::Parse(parser, "Joining:Fleeing",     joinedHeavy3FleeDelay, joinedHeavy3Threshold);

		// Code modifications 
		MemoryTools::MakeRangeJMP<goalUpdateEntrance, goalUpdateExit>(GoalUpdate);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		if (not anyFeatureEnabled) return;

		Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");

		chaserFleeDelay.Log("chaserFleeDelay         ");
		chaserThreshold.Log("chaserThreshold         ");

		joinedRoadblockFleeDelay.Log("joinedRoadblockFleeDelay");
		joinedRoadblockThreshold.Log("joinedRoadblockThreshold");

		heavy3SpeedThreshold.Log("heavy3SpeedThreshold    ");
		heavy3JoiningEnabled.Log("heavy3JoiningEnabled    ");

		if (heavy3JoiningEnabled.current)
			heavy3JoinLimit.Log("heavy3JoinLimit         ");

		joinedHeavy3FleeDelay.Log("joinedH3FleeDelay       ");
		joinedHeavy3Threshold.Log("joinedHeavy3Threshold   ");
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		chaserFleeDelay.SetToHeatState(isRacing, heatLevel);
		chaserThreshold.SetToHeatState(isRacing, heatLevel);

		joinedRoadblockFleeDelay.SetToHeatState(isRacing, heatLevel);
		joinedRoadblockThreshold.SetToHeatState(isRacing, heatLevel);

		heavy3SpeedThreshold .SetToHeatState(isRacing, heatLevel);
		heavy3JoiningEnabled .SetToHeatState(isRacing, heatLevel);
		heavy3JoinLimit      .SetToHeatState(isRacing, heatLevel);
		joinedHeavy3FleeDelay.SetToHeatState(isRacing, heatLevel);
		joinedHeavy3Threshold.SetToHeatState(isRacing, heatLevel);

		baseSpeedThreshold = heavy3SpeedThreshold.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}
}