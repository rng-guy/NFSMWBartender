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
	HeatParameters::OptionalInterval<float> chaserFleeDelays;           // seconds
	HeatParameters::Pair            <int>   chaserChasersThresholds(2); // cars

	HeatParameters::OptionalInterval<float> joinedFleeDelays;           // seconds
	HeatParameters::Pair            <int>   joinedChasersThresholds(2); // cars





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::PursuitReaction
	{
	private:

		const address& heavyStrategy  = *reinterpret_cast<address*>(this->pursuit + 0x194);
		const address& leaderStrategy = *reinterpret_cast<address*>(this->pursuit + 0x198);

		HashContainers::AddressSet chaserVehicles;
		HashContainers::AddressSet joinedVehicles;

		HashContainers::AddressMap<float> chaserVehicleToTimestamp;
		HashContainers::AddressMap<float> joinedVehicleToTimestamp;
		HashContainers::AddressMap<float> strategyVehicleToTimestamp;


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
			const address copVehicle,
			const address strategy
		) {
			if (not Globals::playerHeatLevelKnown) return;

			const float strategyDuration = (strategy) ? *reinterpret_cast<float*>(strategy + 0x8) : 1.f;
			this->ScheduleVehicle(copVehicle, strategyDuration, this->strategyVehicleToTimestamp);
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


		void ReviewJoinedVehicle(const address copVehicle)
		{
			if (not Globals::playerHeatLevelKnown)       return;
			if (not joinedFleeDelays.isEnableds.current) return;

			this->ScheduleVehicle(copVehicle, joinedFleeDelays.GetRandomValue(), this->joinedVehicleToTimestamp);
		}


		void ReviewAllJoinedVehicles()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing joined vehicles");

			this->joinedVehicleToTimestamp.clear();

			for (const address copVehicle : this->joinedVehicles)
				this->ReviewJoinedVehicle(copVehicle);
		}


		bool FleeingPermitted(const int chasersThreshold) const
		{
			return (static_cast<int>(this->chaserVehicles.size()) > chasersThreshold);
		}


		void CheckVolatileVehicles
		(
			HashContainers::AddressSet&        vehicles,
			HashContainers::AddressMap<float>& vehicleToTimestamp,
			const int                          chasersThreshold
		) 
			const
		{
			if (vehicleToTimestamp.empty())                   return;
			if (not this->FleeingPermitted(chasersThreshold)) return;

			auto iterator = vehicleToTimestamp.begin();

			while (iterator != vehicleToTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeVehicleBail(iterator->first);
					vehicles.erase(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Vehicle", iterator->first, "expired");

					iterator = vehicleToTimestamp.erase(iterator);

					if (not this->FleeingPermitted(chasersThreshold))
					{
						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[FLE] Fleeing suspended");

						break;
					}
				}
				else ++iterator;
			}
		}


		void CheckStrategyVehicles()
		{
			if (this->strategyVehicleToTimestamp.empty()) return;

			auto iterator = this->strategyVehicleToTimestamp.begin();

			while (iterator != this->strategyVehicleToTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeVehicleBail(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Vehicle", iterator->first, "expired");

					iterator = this->strategyVehicleToTimestamp.erase(iterator);
				}
				else ++iterator;
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
			this->CheckStrategyVehicles();

			this->CheckVolatileVehicles(this->chaserVehicles, this->chaserVehicleToTimestamp, chaserChasersThresholds.current);
			this->CheckVolatileVehicles(this->joinedVehicles, this->joinedVehicleToTimestamp, joinedChasersThresholds.current);
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewAllChaserVehicles();
			this->ReviewAllJoinedVehicles();
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
				this->ScheduleStrategyVehicle(copVehicle, this->heavyStrategy);
				return;

			case CopLabel::LEADER:
				this->ScheduleStrategyVehicle(copVehicle, this->leaderStrategy);
				return;

			case CopLabel::CHASER:
				this->chaserVehicles.insert(copVehicle);
				this->ReviewChaserVehicle  (copVehicle);
				return;

			case CopLabel::ROADBLOCK:
				this->joinedVehicles.insert(copVehicle);
				this->ReviewJoinedVehicle  (copVehicle);
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
				[[fallthrough]];

			case CopLabel::LEADER:
				this->strategyVehicleToTimestamp.erase(copVehicle);
				return;

			case CopLabel::CHASER:
				this->chaserVehicles          .erase(copVehicle);
				this->chaserVehicleToTimestamp.erase(copVehicle);
				return;

			case CopLabel::ROADBLOCK:
				this->joinedVehicles          .erase(copVehicle);
				this->joinedVehicleToTimestamp.erase(copVehicle);
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
		parser.LoadFile(HeatParameters::configPathAdvanced + "CarSpawns.ini");

		// Heat parameters
		HeatParameters::ParseOptional<float, int>(parser, "Joining:Fleeing", {joinedFleeDelays, 1.f}, {joinedChasersThresholds, 0});
		HeatParameters::ParseOptional<float, int>(parser, "Chasers:Fleeing", {chaserFleeDelays, 1.f}, {chaserChasersThresholds, 0});

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

		chaserFleeDelays       .SetToHeat(isRacing, heatLevel);
		chaserChasersThresholds.SetToHeat(isRacing, heatLevel);

		joinedFleeDelays       .SetToHeat(isRacing, heatLevel);
		joinedChasersThresholds.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}