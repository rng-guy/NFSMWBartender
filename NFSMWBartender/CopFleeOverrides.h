#pragma once

#include <Windows.h>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <array>
#include <map>

#include "Globals.h"
#include "PursuitFeatures.h"
#include "CopSpawnTables.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



namespace CopFleeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat Level
	bool  fleeingEnabled = false;
	float minFleeDelay   = 0.f;
	float maxFleeDelay   = 0.f;

	// Free-roam Heat levels
	std::array<bool,  Globals::maxHeatLevel> roamFleeingEnableds = {};
	std::array<float, Globals::maxHeatLevel> roamMinFleeDelays   = {};
	std::array<float, Globals::maxHeatLevel> roamMaxFleeDelays   = {};

	// Racing Heat levels
	std::array<bool,  Globals::maxHeatLevel> raceFleeingEnableds = {};
	std::array<float, Globals::maxHeatLevel> raceMinFleeDelays   = {};
	std::array<float, Globals::maxHeatLevel> raceMaxFleeDelays   = {};





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

		std::unordered_set<address>             fleeingCopVehicles;
		std::unordered_map<address, Assessment> copVehicleToAssessment;


		void UpdateStrategyDurations()
		{
			static address (__thiscall* const GetSupportAttributes)(address)     = (address (__thiscall*)(address))0x418EE0;
			static address (__thiscall* const GetHeavyStrategies)(address, int)  = (address (__thiscall*)(address, int))0x4035e0;
			static size_t (__thiscall* const GetNumHeavyStrategies)(address)     = (size_t (__thiscall*)(address))0x403600;
			static address (__thiscall* const GetLeaderStrategies)(address, int) = (address (__thiscall*)(address, int))0x403660;
			static size_t (__thiscall* const GetNumLeaderStrategies)(address)    = (size_t (__thiscall*)(address))0x403680;

			// SupportAttributes
			const address supportAttributes = GetSupportAttributes(this->pursuit - 0x48);

			if (not supportAttributes)
			{
				this->heavyStrategyDuration  = 0.f;
				this->leaderStrategyDuration = 0.f;

				if constexpr (Globals::loggingEnabled)
					Globals::Log("WARNING: [FLE] Invalid SupportAttributes pointer in", this->pursuit);

				return;
			}

			// HeavyStrategy 3
			static constexpr int rammingStrategy    = 3;
			const address        heavyStrategies    = GetHeavyStrategies(supportAttributes, 0);
			const size_t         numHeavyStrategies = GetNumHeavyStrategies(supportAttributes);

			if (heavyStrategies and (numHeavyStrategies > 0))
			{
				for (size_t strategyID = 0; strategyID < numHeavyStrategies; strategyID++)
				{
					if (*(int*)(heavyStrategies + strategyID * 0x10) == rammingStrategy)
					{
						this->heavyStrategyDuration = *(float*)(heavyStrategies + strategyID * 0x10 + 0x8);
						break;
					}
				}
			}
			else this->heavyStrategyDuration = 0.f;

			// LeaderStrategies
			const address leaderStrategies    = GetLeaderStrategies(supportAttributes, 0);
			const size_t  numLeaderStrategies = GetNumLeaderStrategies(supportAttributes);

			this->leaderStrategyDuration = (leaderStrategies and (numLeaderStrategies > 0)) ? *(float*)(leaderStrategies + 0x8) : 0.f;

			if constexpr (Globals::loggingEnabled)
			{
				Globals::Log(this->pursuit, "[FLE] HeavyStrategy 3 duration:", this->heavyStrategyDuration);
				Globals::Log(this->pursuit, "[FLE] LeaderStrategy duration:",  this->leaderStrategyDuration);
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

			default:
				return false;
			}
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
				flagVehicle = (this->heavyStrategyDuration > 0.f);
				if (flagVehicle) timeUntilFlee = this->heavyStrategyDuration;
				break;

			case CopLabel::LEADER:
				flagVehicle = (this->leaderStrategyDuration > 0.f);
				if (flagVehicle) timeUntilFlee = this->leaderStrategyDuration;
				break;

			case CopLabel::CHASER:
				flagVehicle = (fleeingEnabled and (not CopSpawnTables::pursuitSpawnTable->IsInTable(assessment.copType)));
				if (flagVehicle) timeUntilFlee = this->prng.GetFloat(maxFleeDelay, minFleeDelay);
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

			const address copAIVehicle = *(address*)(copVehicle + 0x54);
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

		MembershipManager(const address pursuit) : pursuit(pursuit) {}


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





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	void ParseTimers
	(
		ConfigParser::Parser&                     parser,
		const std::string&                        format,
		std::array<bool,  Globals::maxHeatLevel>& fleeingEnableds,
		std::array<float, Globals::maxHeatLevel>& minFleeDelays,
		std::array<float, Globals::maxHeatLevel>& maxFleeDelays
	) {
		fleeingEnableds = parser.ParseParameterTable
		(
			"Fleeing:Timers",
			format,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minFleeDelays, {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxFleeDelays, {}, 0.f)
		);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
			minFleeDelays[heatLevel - 1] = std::min(minFleeDelays[heatLevel - 1], maxFleeDelays[heatLevel - 1]);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		parser.LoadFile(Globals::configPathAdvanced + "Cars.ini");

		// Free-roam timers
		ParseTimers
		(
			parser,
			Globals::configFormatRoam,
			roamFleeingEnableds,
			roamMinFleeDelays,
			roamMaxFleeDelays
		);

		// Racing timers
		ParseTimers
		(
			parser,
			Globals::configFormatRace,
			raceFleeingEnableds,
			raceMinFleeDelays,
			raceMaxFleeDelays
		);

		featureEnabled = true;
	}



	void SetToHeat
	(
		const size_t heatLevel,
		const bool   isRacing
	) {
		if (not featureEnabled) return;

		if (isRacing)
		{
			fleeingEnabled = raceFleeingEnableds[heatLevel - 1];

			if (fleeingEnabled)
			{
				minFleeDelay = raceMinFleeDelays[heatLevel - 1];
				maxFleeDelay = raceMaxFleeDelays[heatLevel - 1];
			}
		}
		else
		{
			fleeingEnabled = roamFleeingEnableds[heatLevel - 1];

			if (fleeingEnabled)
			{
				minFleeDelay = roamMinFleeDelays[heatLevel - 1];
				maxFleeDelay = roamMaxFleeDelays[heatLevel - 1];
			}
		}

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogDashed("[FLE] Updating CopFleeOverrides");
			Globals::LogIndent("[FLE] Fleeing now", (fleeingEnabled) ? "enabled" : "disabled");

			if (fleeingEnabled)
			{
				Globals::LogIndent("[FLE] minFleeDelay           :", minFleeDelay);
				Globals::LogIndent("[FLE] maxFleeDelay           :", maxFleeDelay);
			}
		}
	}
}