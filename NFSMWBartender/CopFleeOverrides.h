#pragma once

#include <initializer_list>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"
#include "PursuitFeatures.h"
#include "CopSpawnTables.h"



namespace CopFleeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::OptionalInterval fleeDelays;
	HeatParameters::Pair<int>        chasersThresholds(2);





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::PursuitReaction
	{
	private:

		const address& heavyStrategy  = *((address*)(this->pursuit + 0x194));
		const address& leaderStrategy = *((address*)(this->pursuit + 0x198));

		HashContainers::AddressSet        activeChaserVehicles;
		HashContainers::AddressMap<float> chaserVehicleToFleeTimestamp;
		HashContainers::AddressMap<float> supportVehicleToFleeTimestamp;
		
		
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


		static bool IsChaserMember(const address copVehicle)
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return CopSpawnTables::chaserSpawnTables.current->Contains(copType);
		}


		void ReviewChaser(const address copVehicle)
		{
			if ((not fleeDelays.isEnableds.current) or this->IsChaserMember(copVehicle)) return;

			const float fleeDelay     = fleeDelays.GetRandomValue();
			const float fleeTimestamp = PursuitFeatures::simulationTime + fleeDelay;
			const bool  wasNew        = this->chaserVehicleToFleeTimestamp.insert({copVehicle, fleeTimestamp}).second;

			if constexpr (Globals::loggingEnabled)
			{
				if (wasNew)
					Globals::logger.Log(this->pursuit, "[FLE]", copVehicle, "expires in", fleeDelay);

				else
					Globals::logger.Log("WARNING: [FLE]", copVehicle, "already scheduled");
			}
		}


		void ReviewAllChasers()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing all chaser vehicles");

			this->chaserVehicleToFleeTimestamp.clear();

			for (const address copVehicle : this->activeChaserVehicles)
				this->ReviewChaser(copVehicle);
		}


		void ScheduleSupport
		(
			const address copVehicle, 
			const float   strategyDuration
		) {
			const float fleeTimestamp = PursuitFeatures::simulationTime + strategyDuration;
			const bool  wasNew        = this->supportVehicleToFleeTimestamp.insert({copVehicle, fleeTimestamp}).second;

			if constexpr (Globals::loggingEnabled)
			{
				if (wasNew)
					Globals::logger.Log(this->pursuit, "[FLE]", copVehicle, "expires in", strategyDuration);

				else
					Globals::logger.Log("WARNING: [FLE]", copVehicle, "already scheduled");
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
				this->ScheduleSupport(copVehicle, this->GetStrategyDuration(this->heavyStrategy));
				break;

			case CopLabel::LEADER:
				this->ScheduleSupport(copVehicle, this->GetStrategyDuration(this->leaderStrategy));
				break;

			case CopLabel::CHASER:
				this->ReviewChaser(copVehicle);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Untrackable label", (int)copLabel, "for", copVehicle, "in", this->pursuit);
			}
		}


		bool MayAnotherChaserFlee() const
		{
			return ((int)(this->activeChaserVehicles.size()) > chasersThresholds.current);
		}


		void CheckChaserTimestamps()
		{
			if (this->chaserVehicleToFleeTimestamp.empty()) return;
			if (not this->MayAnotherChaserFlee())           return;
			
			auto iterator = this->chaserVehicleToFleeTimestamp.begin();

			while (iterator != this->chaserVehicleToFleeTimestamp.end())
			{
				if (PursuitFeatures::simulationTime >= iterator->second)
				{
					PursuitFeatures::MakeVehicleBail(iterator->first);
					this->activeChaserVehicles.erase(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Chaser vehicle", iterator->first, "expired");

					iterator = this->chaserVehicleToFleeTimestamp.erase(iterator);

					if (not this->MayAnotherChaserFlee())
					{
						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[FLE] Fleeing suspended");		

						break;
					}
				}
				else ++iterator;
			}
		}


		void CheckSupportTimestamps()
		{
			if (this->supportVehicleToFleeTimestamp.empty()) return;

			auto iterator = this->supportVehicleToFleeTimestamp.begin();

			while (iterator != this->supportVehicleToFleeTimestamp.end())
			{
				if (PursuitFeatures::simulationTime >= iterator->second)
				{
					PursuitFeatures::MakeVehicleBail(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE] Support vehicle", iterator->first, "expired");

					iterator = this->supportVehicleToFleeTimestamp.erase(iterator);
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
			this->CheckChaserTimestamps();
			this->CheckSupportTimestamps();
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewAllChasers();
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
			else this->supportVehicleToFleeTimestamp.erase(copVehicle);
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
		HeatParameters::Parse<int>(parser, "Chasers:Fleeing", fleeDelays, {chasersThresholds, 0});

		// Code caves
		MemoryTools::DigCodeCave(UpdateFormation, updateFormationEntrance, updateFormationExit);

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

		fleeDelays       .SetToHeat(isRacing, heatLevel);
		chasersThresholds.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			if (fleeDelays.isEnableds.current)
			{
				Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");
				fleeDelays.Log("fleeDelay               ");
				Globals::logger.LogLongIndent("chasersThreshold        ", chasersThresholds.current);
			}
		}
	}
}