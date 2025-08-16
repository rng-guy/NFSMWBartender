#pragma once

#include "Globals.h"
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
	HeatParameters::Pair<float> minFleeDelays  (0.f);   // seconds
	HeatParameters::Pair<float> maxFleeDelays  (0.f);





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::CopVehicleReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:

		enum class Status
		{
			MEMBER,
			FLAGGED,
			FLEEING
		};

		struct Assessment
		{
			const address  copVehicle;
			const CopLabel copLabel;
			Status         status;
		};

		const address pursuit;

		float heavyStrategyDuration  = 0.f;
		float leaderStrategyDuration = 0.f;

		Globals::AddressMap<Assessment> copVehicleToAssessment;
		Globals::AddressMap<float>      copVehicleToFleeTimestamp;
		Globals::AddressSet             fleeingCopVehicles;
		

		void UpdateStrategyDurations()
		{
			this->heavyStrategyDuration  = 0.f;
			this->leaderStrategyDuration = 0.f;

			// PursuitSupportAttributes node
			static address (__thiscall* const GetSupportNode)(address) = (address (__thiscall*)(address))0x418EE0;

			const address supportNode = GetSupportNode(this->pursuit - 0x48);

			if (not supportNode)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Invalid SupportNode pointer in", this->pursuit);

				return;
			}

			// HeavyStrategy 3
			static address (__thiscall* const GetHeavyStrategies)   (address, int) = (address (__thiscall*)(address, int))0x4035E0;
			static size_t  (__thiscall* const GetNumHeavyStrategies)(address)      = (size_t  (__thiscall*)(address))     0x403600;

			const address heavyStrategies    = GetHeavyStrategies(supportNode, 0);
			const size_t  numHeavyStrategies = GetNumHeavyStrategies(supportNode);

			if (heavyStrategies)
			{
				address heavyStrategy = heavyStrategies;

				for (size_t strategyID = 0; strategyID < numHeavyStrategies; strategyID++)
				{
					if (*((int*)heavyStrategy) == 3) // is ramming strategy
					{
						this->heavyStrategyDuration = *((float*)(heavyStrategy + 0x8));

						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[FLE] HeavyStrategy 3 duration:", this->heavyStrategyDuration);

						break;
					}

					heavyStrategy += 0x10;
				}
			}

			// LeaderStrategies
			static address (__thiscall* const GetLeaderStrategies)   (address, int) = (address (__thiscall*)(address, int))0x403660;
			static size_t  (__thiscall* const GetNumLeaderStrategies)(address)      = (size_t  (__thiscall*)(address))     0x403680;

			const address leaderStrategies    = GetLeaderStrategies(supportNode, 0);
			const size_t  numLeaderStrategies = GetNumLeaderStrategies(supportNode);

			if (leaderStrategies and (numLeaderStrategies > 0))
			{
				this->leaderStrategyDuration = *((float*)(leaderStrategies + 0x8));

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE] LeaderStrategy duration:", this->leaderStrategyDuration);
			}
		}


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


		static bool IsInPursuitTable(const address copVehicle)
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return CopSpawnTables::pursuitSpawnTables.current->Contains(copType);
		}


		void Review(Assessment& assessment) 
		{
			if (not this->IsHeatLevelKnown())        return;
			if (assessment.status != Status::MEMBER) return;

			bool  flagVehicle   = false;
			float timeUntilFlee = 0.f;

			switch (assessment.copLabel)
			{
			case CopLabel::HEAVY:
				flagVehicle   = true;
				timeUntilFlee = this->heavyStrategyDuration;
				break;

			case CopLabel::LEADER:
				flagVehicle   = true;
				timeUntilFlee = this->leaderStrategyDuration;
				break;

			case CopLabel::CHASER:
				flagVehicle = (fleeingEnableds.current and (not this->IsInPursuitTable(assessment.copVehicle)));
				if (flagVehicle) timeUntilFlee = Globals::prng.Generate<float>(minFleeDelays.current, maxFleeDelays.current);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Unknown label", (int)(assessment.copLabel), "in", this->pursuit);
			}

			if (flagVehicle)
			{
				assessment.status = Status::FLAGGED;
				this->copVehicleToFleeTimestamp.insert({assessment.copVehicle, *(this->simulationTime) + timeUntilFlee});

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE]", assessment.copVehicle, "fleeing in", timeUntilFlee);
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
				case (Status::FLAGGED):
					this->copVehicleToFleeTimestamp.erase(pair.first);
					pair.second.status = Status::MEMBER;
					[[fallthrough]];

				case (Status::MEMBER):
					this->Review(pair.second);
				}
			}
		}


		void MakeFlee(const address copVehicle) const
		{
			static void (__thiscall* const StartFlee)(address) = (void (__thiscall*)(address))0x423370;

			const address copAIVehicle = *((address*)(copVehicle + 0x54));

			if (copAIVehicle)
				StartFlee(copAIVehicle + 0x70C);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Invalid AIVehicle for", copVehicle, "in", this->pursuit);
		}


		void CheckSchedule()
		{
			if (this->copVehicleToFleeTimestamp.size() > 0)
			{
				const float simulationTime = *(this->simulationTime);
				auto        iterator       = this->copVehicleToFleeTimestamp.begin();

				while (iterator != this->copVehicleToFleeTimestamp.end())
				{
					if (simulationTime >= iterator->second)
					{
						this->copVehicleToAssessment.at(iterator->first).status = Status::FLEEING;
						this->fleeingCopVehicles.insert(iterator->first);

						if constexpr (Globals::loggingEnabled)
							Globals::logger.Log(this->pursuit, "[FLE]", iterator->first, "now fleeing");

						iterator = this->copVehicleToFleeTimestamp.erase(iterator);
					}
					else iterator++;
				}
			}

			for (const address copVehicle : this->fleeingCopVehicles) 
				this->MakeFlee(copVehicle);
		}



	public:

		explicit MembershipManager(const address pursuit) : pursuit(pursuit) {}


		void UpdateOnGameplay() override 
		{
			this->CheckSchedule();
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewAll();
		}


		void UpdateOncePerHeatLevel() override
		{
			this->UpdateStrategyDurations();
		}


		void ProcessAddition
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsTrackable(copLabel)) return;

			const auto addedVehicle = this->copVehicleToAssessment.insert({copVehicle, {copVehicle, copLabel, Status::MEMBER}});

			if (addedVehicle.second)
				this->Review(addedVehicle.first->second);

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Already-tracked vehicle", copVehicle, "in", this->pursuit);
		}


		void ProcessRemoval
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
				switch (foundVehicle->second.status)
				{
				case Status::FLAGGED:
					this->copVehicleToFleeTimestamp.erase(copVehicle);
					break;

				case Status::FLEEING:
					this->fleeingCopVehicles.erase(copVehicle);
				}

				this->copVehicleToAssessment.erase(foundVehicle);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [FLE] Unknown vehicle", copVehicle, "in", this->pursuit);
		}
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini");

		// Heat parameters
		for (const bool forRaces : {false, true})
		{
			fleeingEnableds.Get(forRaces) = parser.ParseParameterTable<float, float>
			(
				"Fleeing:Timers",
				(forRaces) ? HeatParameters::configFormatRace : HeatParameters::configFormatRoam,
				HeatParameters::Format<float>(minFleeDelays.Get(forRaces), {}, 0.f),
				HeatParameters::Format<float>(maxFleeDelays.Get(forRaces), {}, 0.f)
			);
		}

		HeatParameters::CheckIntervals<float>(minFleeDelays, maxFleeDelays);

		return (featureEnabled = true);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		fleeingEnableds.SetToHeat(isRacing, heatLevel);

		if (fleeingEnableds.current)
		{
			minFleeDelays.SetToHeat(isRacing, heatLevel);
			maxFleeDelays.SetToHeat(isRacing, heatLevel);
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [FLE] CopFleeOverrides");
			Globals::logger.LogLongIndent("Fleeing", (fleeingEnableds.current) ? "enabled" : "disabled");

			if (fleeingEnableds.current)
			{
				Globals::logger.LogLongIndent("minFleeDelay            ", minFleeDelays.current);
				Globals::logger.LogLongIndent("maxFleeDelay            ", maxFleeDelays.current);
			}
		}
	}
}