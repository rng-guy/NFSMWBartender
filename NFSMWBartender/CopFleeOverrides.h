#pragma once

#include <initializer_list>

#include "Globals.h"
#include "HashContainers.h"
#include "PursuitFeatures.h"
#include "CopSpawnTables.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace CopFleeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::OptionalInterval fleeDelays;
	HeatParameters::Pair<int>        minNumChasersRemainings(2);





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::PursuitReaction
	{
	private:

		const address& heavyStrategy  = *((address*)(this->pursuit + 0x194));
		const address& leaderStrategy = *((address*)(this->pursuit + 0x198));

		HashContainers::AddressSet        activeChaserVehicles;
		HashContainers::AddressMap<float> chaserVehicleToFleeTimestamp;
		HashContainers::AddressMap<float> strategyVehicleToFleeTimestamp;
		
		
		static bool IsLabelTrackable(const CopLabel copLabel)
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				[[fallthrough]];

			case CopLabel::LEADER:
				[[fallthrough]];

			case CopLabel::CHASER:
				return true;
			}

			return false;
		}


		static bool IsChaserVehicleMember(const address copVehicle)
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return CopSpawnTables::chaserSpawnTables.current->Contains(copType);
		}


		void ReviewChaserVehicle(const address copVehicle)
		{
			if ((not fleeDelays.isEnableds.current) or this->IsChaserVehicleMember(copVehicle)) return;

			const float timeUntilFlee = fleeDelays.GetRandomValue();
			const bool  wasNew        = this->chaserVehicleToFleeTimestamp.insert({copVehicle, Globals::simulationTime + timeUntilFlee}).second;

			if constexpr (Globals::loggingEnabled)
			{
				if (wasNew)
					Globals::logger.Log(this->pursuit, "[FLE] Chaser vehicle", copVehicle, "expires in", timeUntilFlee);

				else
					Globals::logger.Log("WARNING: [FLE] Chaser vehicle", copVehicle, "already scheduled");
			}
		}


		void ReviewAllChaserVehicles()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing all active chaser vehicles");

			this->chaserVehicleToFleeTimestamp.clear();

			for (const address copVehicle : this->activeChaserVehicles)
				this->ReviewChaserVehicle(copVehicle);
		}


		void ScheduleStrategyVehicle
		(
			const address copVehicle, 
			const float   strategyDuration
		) {
			const bool wasNew = this->strategyVehicleToFleeTimestamp.insert({copVehicle, Globals::simulationTime + strategyDuration}).second;

			if constexpr (Globals::loggingEnabled)
			{
				if (wasNew)
					Globals::logger.Log(this->pursuit, "[FLE] Strategy vehicle", copVehicle, "expires in", strategyDuration);

				else
					Globals::logger.Log("WARNING: [FLE] Strategy vehicle", copVehicle, "already scheduled");
			}
		}


		static float GetStrategyDuration(const address strategy)
		{
			return (strategy) ? *((float*)(strategy + 0x8)) : 1.f;
		}


		void ReviewVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) {
			if (not PursuitFeatures::heatLevelKnown) return;

			switch (copLabel)
			{
			case CopLabel::HEAVY:
				this->ScheduleStrategyVehicle(copVehicle, this->GetStrategyDuration(this->heavyStrategy));
				break;

			case CopLabel::LEADER:
				this->ScheduleStrategyVehicle(copVehicle, this->GetStrategyDuration(this->leaderStrategy));
				break;

			case CopLabel::CHASER:
				this->ReviewChaserVehicle(copVehicle);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Untrackable label", (int)copLabel, "for", copVehicle, "in", this->pursuit);
			}
		}


		bool MayAnotherChaserVehicleFlee() const
		{
			return ((int)(this->activeChaserVehicles.size()) > minNumChasersRemainings.current);
		}


		void CheckChaserVehicleTimestamps()
		{
			if (this->chaserVehicleToFleeTimestamp.empty()) return;
			if (not this->MayAnotherChaserVehicleFlee())    return;
			
			auto iterator = this->chaserVehicleToFleeTimestamp.begin();

			while (iterator != this->chaserVehicleToFleeTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeVehicleBail(iterator->first);
					this->activeChaserVehicles.erase(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Chaser vehicle", iterator->first, "expired");

					iterator = this->chaserVehicleToFleeTimestamp.erase(iterator);

					if (not this->MayAnotherChaserVehicleFlee())
					{
						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[FLE] Suspending fleeing");		

						break;
					}
				}
				else ++iterator;
			}
		}


		void CheckStrategyVehicleTimestamps()
		{
			if (this->strategyVehicleToFleeTimestamp.empty()) return;

			auto iterator = this->strategyVehicleToFleeTimestamp.begin();

			while (iterator != this->strategyVehicleToFleeTimestamp.end())
			{
				if (Globals::simulationTime >= iterator->second)
				{
					this->MakeVehicleBail(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Strategy vehicle", iterator->first, "expired");

					iterator = this->strategyVehicleToFleeTimestamp.erase(iterator);
				}
				else ++iterator;
			}
		}



	public:

		explicit MembershipManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) {}


		void UpdateOnGameplay() override 
		{
			this->CheckChaserVehicleTimestamps();
			this->CheckStrategyVehicleTimestamps();
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewAllChaserVehicles();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsLabelTrackable(copLabel)) return;

			if (copLabel == CopLabel::CHASER)
				this->activeChaserVehicles.insert(copVehicle);

			this->ReviewVehicle(copVehicle, copLabel);
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsLabelTrackable(copLabel)) return;

			if (copLabel == CopLabel::CHASER)
			{
				if (this->activeChaserVehicles.erase(copVehicle))
					this->chaserVehicleToFleeTimestamp.erase(copVehicle);
			}
			else this->strategyVehicleToFleeTimestamp.erase(copVehicle);
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address updateFormationEntrance = 0x443917;
	constexpr address updateFormationExit     = 0x44391C;

	__declspec(naked) void UpdateFormation()
	{
		__asm
		{
			mov edi, dword ptr [ebp]
			test edi, edi
			je conclusion // invalid AIVehiclePursuit

			mov eax, dword ptr [edi - 0x758 + 0xC4]
			cmp eax, 0x88C018A9 // AIGoalFleePursuit

			conclusion:
			jmp dword ptr updateFormationExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini");

		// Heat parameters
		HeatParameters::Parse     (parser, "Chasers:Fleeing",     fleeDelays);
		HeatParameters::Parse<int>(parser, "Chasers:Contingency", {minNumChasersRemainings, 0});

		// Code caves
		MemoryEditor::DigCodeCave(UpdateFormation, updateFormationEntrance, updateFormationExit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		fleeDelays             .SetToHeat(isRacing, heatLevel);
		minNumChasersRemainings.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			if (fleeDelays.isEnableds.current)
			{
				Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");
				fleeDelays.Log("fleeDelay               ");
				Globals::logger.LogLongIndent("minNumChasersRemaining  ", minNumChasersRemainings.current);
			}
		}
	}
}