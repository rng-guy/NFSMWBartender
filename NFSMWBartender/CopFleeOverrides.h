#pragma once

#include <map>

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
		using Schedule = std::multimap<float, address>;



	private:

		enum class Status
		{
			MEMBER,
			FLAGGED,
			FLEEING
		};

		struct Assessment
		{
			address            copVehicle;
			hash               copType;
			CopLabel           copLabel;
			Status             status;
			Schedule::iterator position;
		};

		const address pursuit;

		Schedule waitingToFlee;

		float heavyStrategyDuration  = 0.f;
		float leaderStrategyDuration = 0.f;

		Globals::AddressSet             fleeingCopVehicles;
		Globals::AddressMap<Assessment> copVehicleToAssessment;


		void UpdateStrategyDurations()
		{
			this->heavyStrategyDuration  = 0.f;
			this->leaderStrategyDuration = 0.f;

			// SupportAttributes
			static address (__thiscall* const GetSupportAttributes)(address) = (address (__thiscall*)(address))0x418EE0;

			const address supportAttributes = GetSupportAttributes(this->pursuit - 0x48);

			if (not supportAttributes)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::Log("WARNING: [FLE] Invalid SupportAttributes pointer in", this->pursuit);

				return;
			}

			// HeavyStrategy 3
			static address (__thiscall* const GetHeavyStrategies)(address, int) = (address (__thiscall*)(address, int))0x4035E0;
			static size_t (__thiscall* const GetNumHeavyStrategies)(address)    = (size_t (__thiscall*)(address))0x403600;

			static constexpr int rammingStrategy    = 3;
			const address        heavyStrategies    = GetHeavyStrategies(supportAttributes, 0);
			const size_t         numHeavyStrategies = GetNumHeavyStrategies(supportAttributes);

			if (heavyStrategies)
			{
				address heavyStrategy = heavyStrategies;

				for (size_t strategyID = 0; strategyID < numHeavyStrategies; strategyID++)
				{
					if (*((int*)heavyStrategy) == rammingStrategy)
					{
						this->heavyStrategyDuration = *((float*)(heavyStrategy + 0x8));

						if constexpr (Globals::loggingEnabled)
							Globals::Log(this->pursuit, "[FLE] HeavyStrategy 3 duration:", this->heavyStrategyDuration);

						break;
					}

					heavyStrategy += 0x10;
				}
			}

			// LeaderStrategies
			static address (__thiscall* const GetLeaderStrategies)(address, int) = (address (__thiscall*)(address, int))0x403660;
			static size_t (__thiscall* const GetNumLeaderStrategies)(address)    = (size_t (__thiscall*)(address))0x403680;

			const address leaderStrategies    = GetLeaderStrategies(supportAttributes, 0);
			const size_t  numLeaderStrategies = GetNumLeaderStrategies(supportAttributes);

			if (leaderStrategies and (numLeaderStrategies > 0))
			{
				this->leaderStrategyDuration = *((float*)(leaderStrategies + 0x8));

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[FLE] LeaderStrategy duration:", this->leaderStrategyDuration);
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


		void Review(Assessment& assessment) 
		{
			if (not this->IsHeatLevelKnown())        return;
			if (assessment.status != Status::MEMBER) return;

			bool  flagVehicle;
			float timeUntilFlee;

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
				flagVehicle = (fleeingEnableds.current and (not (CopSpawnTables::pursuitSpawnTables.current->Contains(assessment.copType))));
				if (flagVehicle) timeUntilFlee = Globals::prng.Generate<float>(minFleeDelays.current, maxFleeDelays.current);
				break;

			default:
				flagVehicle = false;

				if constexpr (Globals::loggingEnabled)
					Globals::Log("WARNING: [FLE] Unknown label", (int)(assessment.copLabel), "in", this->pursuit);
			}

			if (not flagVehicle) return;

			assessment.status   = Status::FLAGGED;
			assessment.position = this->waitingToFlee.insert({*(this->simulationTime) + timeUntilFlee, assessment.copVehicle});

			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[FLE]", assessment.copVehicle, "to flee in", timeUntilFlee);
		}


		void ReviewAll()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[FLE] Reviewing contingent");

			for (auto& pair : this->copVehicleToAssessment)
			{
				if (pair.second.copLabel != CopLabel::CHASER) continue;
				if (pair.second.status == Status::FLEEING)    continue;

				if (pair.second.status == Status::FLAGGED)
				{
					this->waitingToFlee.erase(pair.second.position);
					pair.second.status = Status::MEMBER;
				}

				this->Review(pair.second);
			}
		}


		static void MakeFlee(const address copVehicle)
		{
			static void (__thiscall* const StartFlee)(address) = (void (__thiscall*)(address))0x423370;

			const address copAIVehicle = *((address*)(copVehicle + 0x54));
			if (copAIVehicle) StartFlee(copAIVehicle + 0x70C);
		}


		void CheckSchedule()
		{
			const float simulationTime = *(this->simulationTime);
			auto        iterator       = this->waitingToFlee.begin();

			while (iterator != this->waitingToFlee.end())
			{
				if (simulationTime < iterator->first) break;
				Assessment& assessment = this->copVehicleToAssessment.at((iterator++)->second);

				assessment.status = Status::FLEEING;
				this->waitingToFlee.erase(assessment.position);
				this->fleeingCopVehicles.insert(assessment.copVehicle);

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[FLE]", assessment.copVehicle, "now fleeing");
			}

			std::for_each(this->fleeingCopVehicles.begin(), this->fleeingCopVehicles.end(), this->MakeFlee);
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
			const hash     copType,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsTrackable(copLabel)) return;

			this->Review(this->copVehicleToAssessment.insert({copVehicle, {copVehicle, copType, copLabel, Status::MEMBER}}).first->second);
		}


		void ProcessRemoval
		(
			const address  copVehicle,
			const address  copType,
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
					this->waitingToFlee.erase(foundVehicle->second.position);
					break;

				case Status::FLEEING:
					this->fleeingCopVehicles.erase(foundVehicle->second.copVehicle);
				}

				this->copVehicleToAssessment.erase(foundVehicle);
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [FLE] Unknown vehicle", copVehicle, "in", this->pursuit);
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
			Globals::LogIndent("[FLE] CopFleeOverrides");
			Globals::LogLongIndent("Fleeing", (fleeingEnableds.current) ? "enabled" : "disabled");

			if (fleeingEnableds.current)
			{
				Globals::LogLongIndent("minFleeDelay           :", minFleeDelays.current);
				Globals::LogLongIndent("maxFleeDelay           :", maxFleeDelays.current);
			}
		}
	}
}