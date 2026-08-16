#pragma once

#include <functional>

#include "../../Common/Globals.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "CopSpawnTables.hpp"
#include "PursuitFeatures.hpp"



namespace CopFleeOverrides
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[FLE]";
	constexpr LogLiteral logName = "CopFleeOverrides";

	// Heat parameters
	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, chaserFleeDelay, {1.f}); // seconds
	constinit OPTIONAL_HEAT_PARAMETER_VALUE   (int,   chaserThreshold, {0});   // cars

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, joinedRoadblockFleeDelay, {1.f}); // seconds
	constinit OPTIONAL_HEAT_PARAMETER_VALUE   (int,   joinedRoadblockThreshold, {0});   // cars

	constinit HEAT_PARAMETER_VALUE(float, heavy3SpeedThreshold, 25.f, {0.f}); // kph

	constinit HEAT_PARAMETER_VALUE(bool, heavy3JoiningEnabled, false);

	constinit OPTIONAL_HEAT_PARAMETER_VALUE(int, heavy3JoinLimit, {0}); // cars

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, joinedHeavy3FleeDelay, {1.f}); // seconds
	constinit OPTIONAL_HEAT_PARAMETER_VALUE   (int,   joinedHeavy3Threshold, {0});   // cars

	// Conversions
	float baseSpeedThreshold = heavy3SpeedThreshold.current / 3.6f; // metres / second
	float jerkSpeedThreshold = baseSpeedThreshold * .625f;          // metres / second

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
		protected: // members

			size_t numPendingExpired = 0;

			const address    pursuit;
			const LogLiteral vehicleLabel;

			ModContainers::AddressMap<float> copVehicleToTimestamp;

			// Whether currently scheduled cops should be checked for expiration
			std::function<bool ()> ShouldCheckForExpiration = []() -> bool {return true;};

			// Whether a given expired cop vehicle should actually be forced to bail the pursuit
			std::function<bool (const address)> ShouldExpiredVehicleBail = [](const address copVehicle) -> bool {return true;};


		protected: // methods

			SchedulerBase
			(
				const address    pursuit,
				const LogLiteral vehicleLabel
			)
				: pursuit(pursuit), vehicleLabel(vehicleLabel)
			{
			}


			SchedulerBase(SchedulerBase&&)      = delete;
			SchedulerBase(const SchedulerBase&) = delete;

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
						Globals::LogFull(this->pursuit, logTag, copVehicle, "expires in", fleeTimer);

					else Globals::LogError(logTag, copVehicle, "already scheduled");
				}
			}


			bool MakeVehicleBail(const address copVehicle) const
			{
				const address copAIVehiclePursuit = Globals::GetAIVehiclePursuitOfVehicle(copVehicle);
				if (not copAIVehiclePursuit) return false; // should never happen

				const auto StartFlee = AsFunction<void __thiscall (address)>(0x423370);
				StartFlee(copAIVehiclePursuit); // also updates vehicle goal(s) accordingly

				if constexpr (Globals::loggingEnabled)
					Globals::LogFull(this->pursuit, logTag, this->vehicleLabel, copVehicle, "fleeing");

				return true;
			}


		public: // methods

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
		private: // members

			const address& strategy;


		public: // aliases

			using SchedulerBase::ShouldExpiredVehicleBail;


		public: // methods

			StrategyScheduler
			(
				const address    pursuit,
				const LogLiteral vehicleLabel,
				const ptrdiff_t  strategyOffset
			)
				: SchedulerBase(pursuit, vehicleLabel), strategy(AsReference<address>(pursuit + strategyOffset))
			{
			}


			void ReserveCapacity(const size_t numVehicles)
			{
				this->copVehicleToTimestamp.reserve(numVehicles);
			}


			void AddVehicle(const address copVehicle)
			{
				const float strategyDuration = (this->strategy) ? AsReference<float>(this->strategy + 0x8) : 1.f;
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
		public: // members

			// Total number of active "Chasers"
			std::function<size_t ()> GetNumActiveChasers = []() -> size_t {return 8;};

			// Whether a given cop vehicle may be scheduled for expiration
			std::function<bool (const address)> IsSchedulable = [](const address copVehicle) -> bool {return true;};


		private: // members

			ModContainers::AddressSet copVehicles; // for tracking in case of Heat transitions

			const HeatParameters::OptionalInterval<float>& fleeDelay;
			const HeatParameters::OptionalValue   <int>&   fleeThreshold;


		private: // methods

			void ReviewVehicle(const address copVehicle)
			{
				if (not Globals::playerHeatLevelKnown)     return;
				if (not this->IsSchedulable(copVehicle))   return;
				if (not this->fleeDelay.isEnabled.current) return;

				this->ScheduleVehicle(copVehicle, this->fleeDelay.interval.GetRandomValue());
			}


		public: // methods

			PursuitScheduler
			(
				const address                                  pursuit,
				const LogLiteral                               vehicleLabel,
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
			

			void ReserveCapacity(const size_t numVehicles)
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

	class MembershipManager : public PursuitFeatures::Reaction
	{
	private: // aliases

		using StrategyScheduler = Details::StrategyScheduler;
		using PursuitScheduler  = Details::PursuitScheduler;


	private: // members

		bool pursuitTargetKnown = false;

		StrategyScheduler heavyVehicles {this->pursuit, "Heavy",  0x194};
		StrategyScheduler leaderVehicles{this->pursuit, "Leader", 0x198};

		PursuitScheduler chaserVehicles         {this->pursuit, "Chaser",    chaserFleeDelay,          chaserThreshold};
		PursuitScheduler joinedHeavyVehicles    {this->pursuit, "Joined H3", joinedHeavy3FleeDelay,    joinedHeavy3Threshold};
		PursuitScheduler joinedRoadblockVehicles{this->pursuit, "Joined RB", joinedRoadblockFleeDelay, joinedRoadblockThreshold};

		const bool& isJerk = AsReference<bool>(this->pursuit + 0x238);

		inline static constexpr LogLiteral name = "MembershipManager";


	private: // methods

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
			if (not this->MayAnotherHeavyJoin())                  return false;
			if (not Globals::EndSupportGoalOfVehicle(copVehicle)) return false;

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Heavy", copVehicle, "joined");

			this->joinedHeavyVehicles.AddVehicle(copVehicle);

			return true;
		}


		[[nodiscard]] address GetRigidBodyOfPursuitTarget() const
		{
			if (not this->pursuitTargetKnown) return 0x0;

			const address physicsObject = Globals::GetPhysicsObjectOfPursuitTarget(this->pursuit);
			if (not physicsObject) return 0x0; // should never happen

			return AsReference<address>(physicsObject + 0x4C);
		}


		[[nodiscard]] bool ShouldHeavyVehiclesBail() const
		{
			if (Globals::IsPursuitInCooldownMode(this->pursuit)) return true;

			// Check target speed against bail threshold
			if (const address rigidBodyOfTarget = this->GetRigidBodyOfPursuitTarget())
			{
				const auto  GetSpeedXZ     = AsFunction<float __thiscall (address)>(0x6711F0);
				const float speedThreshold = (this->isJerk) ? jerkSpeedThreshold : baseSpeedThreshold;

				return (GetSpeedXZ(rigidBodyOfTarget) < speedThreshold);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::LogError(logTag, "Invalid RigidBody for target in", this->pursuit);

			return false; // should never happen
		}


		void CheckForHeavyCancellation()
		{
			if (this->heavyVehicles.GetNumScheduled() == 0) return;
			if (not this->ShouldHeavyVehiclesBail())        return;

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Bailing HeavyStrategy3");

			this->heavyVehicles.ForceTriggerExpiration();

			if (const address heavyStrategy = this->heavyVehicles.GetStrategy())
			{
				const int strategyID = AsReference<int>(heavyStrategy);

				if (strategyID == 3) // ramming SUVs
					Globals::ClearSupportRequest(this->pursuit);
			}
		}


	public: // members

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


	public: // methods

		explicit MembershipManager(const address pursuit) : PursuitFeatures::Reaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('+', this, this->name);

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
			this->heavyVehicles .ReserveCapacity(10);
			this->leaderVehicles.ReserveCapacity(10);

			this->chaserVehicles         .ReserveCapacity(50);
			this->joinedHeavyVehicles    .ReserveCapacity(10);
			this->joinedRoadblockVehicles.ReserveCapacity(10);
		}


		~MembershipManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('-', this, this->name);
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
				Globals::LogFull(this->pursuit, logTag, "Reviewing all vehicles");

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
			Globals::LogConfig(logTag, logName);

		// Heat parameters (first file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "CarSpawns.ini");

		HeatParameters::Parse(parser, "Chasers:Fleeing", chaserFleeDelay, chaserThreshold);

		// Heat parameters (second file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "Roadblocks.ini");

		HeatParameters::Parse(parser, "Joining:Fleeing", joinedRoadblockFleeDelay, joinedRoadblockThreshold);

		// Heat parameters (third file)
		parser.LoadFile(HeatParameters::configPathAdvanced, "Strategies.ini");

		HeatParameters::Parse(parser, "Heavy3:Cancellation", heavy3SpeedThreshold);

		HeatParameters::Parse(parser, "Heavy3:Joining", heavy3JoiningEnabled);

		HeatParameters::Parse(parser, "Joining:Limit", heavy3JoinLimit);

		HeatParameters::Parse(parser, "Joining:Fleeing", joinedHeavy3FleeDelay, joinedHeavy3Threshold);

		// Code modifications 
		MemoryTools::MakeRangeJMP<goalUpdateEntrance, goalUpdateExit>(GoalUpdate);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		chaserFleeDelay.SetToHeatState(state);
		chaserThreshold.SetToHeatState(state);

		joinedRoadblockFleeDelay.SetToHeatState(state);
		joinedRoadblockThreshold.SetToHeatState(state);

		heavy3SpeedThreshold.SetToHeatState(state);

		heavy3JoiningEnabled.SetToHeatState(state);

		heavy3JoinLimit.SetToHeatState(state);

		joinedHeavy3FleeDelay.SetToHeatState(state);
		joinedHeavy3Threshold.SetToHeatState(state);

		baseSpeedThreshold = heavy3SpeedThreshold.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;
	}
}