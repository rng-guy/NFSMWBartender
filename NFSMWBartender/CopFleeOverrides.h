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
	HeatParameters::Pair<bool>  fleeingEnableds(false);
	HeatParameters::Pair<float> minFleeDelays  (1.f);   // seconds
	HeatParameters::Pair<float> maxFleeDelays  (1.f);





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::PursuitReaction
	{
	private:

		const address& heavyStrategy  = *((address*)(this->pursuit + 0x194));
		const address& leaderStrategy = *((address*)(this->pursuit + 0x198));

		enum class Status
		{
			MEMBER,
			FLAGGED,
			FLEEING
		};

		struct Assessment
		{
			const CopLabel copLabel;
			Status         status;
		};

		HashContainers::AddressMap<Assessment> copVehicleToAssessment;
		HashContainers::AddressMap<float>      copVehicleToFleeTimestamp;
		
		
		static bool IsTrackable(const CopLabel copLabel)
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


		static float GetStrategyDuration(const address strategy)
		{
			return (strategy) ? *((float*)(strategy + 0x8)) : 1.f;
		}


		static bool IsInChasersTable(const address copVehicle)
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return CopSpawnTables::chaserSpawnTables.current->Contains(copType);
		}


		void Review(std::pair<const address, Assessment>& pair) 
		{
			if (not this->IsHeatLevelKnown())         return;
			if (pair.second.status != Status::MEMBER) return;

			bool  flagVehicle   = false;
			float timeUntilFlee = 0.f;

			switch (pair.second.copLabel)
			{
			case CopLabel::HEAVY:
				flagVehicle   = true;
				timeUntilFlee = GetStrategyDuration(this->heavyStrategy);
				break;

			case CopLabel::LEADER:
				flagVehicle   = true;
				timeUntilFlee = GetStrategyDuration(this->leaderStrategy);
				break;

			case CopLabel::CHASER:
				flagVehicle = (fleeingEnableds.current and (not this->IsInChasersTable(pair.first)));
				if (flagVehicle) timeUntilFlee = Globals::prng.GenerateNumber<float>(minFleeDelays.current, maxFleeDelays.current);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Unknown label", (int)(pair.second.copLabel), "in", this->pursuit);
			}

			if (flagVehicle)
			{
				pair.second.status = Status::FLAGGED;
				this->copVehicleToFleeTimestamp.insert({pair.first, PursuitFeatures::simulationTime + timeUntilFlee});

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE]", pair.first, "fleeing in", timeUntilFlee);
			}
		}


		void ReviewAll()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing contingent");

			for (auto& pair : this->copVehicleToAssessment)
			{
				if (pair.second.copLabel != CopLabel::CHASER) continue;

				switch (pair.second.status)
				{
				case Status::FLAGGED:
					this->copVehicleToFleeTimestamp.erase(pair.first);
					pair.second.status = Status::MEMBER;
					[[fallthrough]];

				case Status::MEMBER:
					this->Review(pair);
				}
			}
		}


		void CheckTimestamps()
		{
			if (this->copVehicleToFleeTimestamp.empty()) return;
			
			auto iterator = this->copVehicleToFleeTimestamp.begin();

			while (iterator != this->copVehicleToFleeTimestamp.end())
			{
				if (PursuitFeatures::simulationTime >= iterator->second)
				{
					this->copVehicleToAssessment.at(iterator->first).status = Status::FLEEING;
					PursuitFeatures::MakeVehicleFlee(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE]", iterator->first, "now fleeing");

					iterator = this->copVehicleToFleeTimestamp.erase(iterator);
				}
				else ++iterator;
			}
		}



	public:

		explicit MembershipManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) {}


		void UpdateOnGameplay() override 
		{
			this->CheckTimestamps();
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewAll();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsTrackable(copLabel)) return;

			const auto addedVehicle = this->copVehicleToAssessment.insert({copVehicle, {copLabel, Status::MEMBER}});

			if (addedVehicle.second)
				this->Review(*(addedVehicle.first));

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Already-tracked vehicle", copVehicle, "in", this->pursuit);
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsTrackable(copLabel)) return;

			const auto foundVehicle = this->copVehicleToAssessment.find(copVehicle);

			if (foundVehicle != this->copVehicleToAssessment.end())
			{
				if (foundVehicle->second.status == Status::FLAGGED)
					this->copVehicleToFleeTimestamp.erase(copVehicle);

				this->copVehicleToAssessment.erase(foundVehicle);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Unknown vehicle", copVehicle, "in", this->pursuit);
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address updateFormationEntrance = 0x443917;
	constexpr address updateFormationExit     = 0x44391C;

	__declspec(naked) void UpdateFormation()
	{
		__asm
		{
			mov edi, [ebp]
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
		HeatParameters::ParseOptionalInterval(parser, "Chasers:Fleeing", fleeingEnableds, minFleeDelays, maxFleeDelays);

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

		fleeingEnableds.SetToHeat(isRacing, heatLevel);
		minFleeDelays  .SetToHeat(isRacing, heatLevel);
		maxFleeDelays  .SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			if (fleeingEnableds.current)
			{
				Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");
				Globals::logger.LogLongIndent("fleeDelay               ", minFleeDelays.current, "to", maxFleeDelays.current);
			}
		}
	}
}