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

	// General Heat levels
	std::array<bool,  Globals::maxHeatLevel> fleeingEnableds = {};
	std::array<float, Globals::maxHeatLevel> minFleeDelays   = {};
	std::array<float, Globals::maxHeatLevel> maxFleeDelays   = {};





	// MembershipManager class ----------------------------------------------------------------------------------------------------------------------

	class MembershipManager : public PursuitFeatures::CopVehicleReaction
	{
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
			Status             status;
			Schedule::iterator position;
		};

		const address pursuit;

		Schedule waitingToFlee;

		std::unordered_set<address>             fleeingCopVehicles;
		std::unordered_map<address, Assessment> copVehicleToAssessment;


		void Review(Assessment& assessment)
		{
			if (not this->IsHeatLevelKnown())         return;
			if (assessment.status != Status::MEMBER) return;

			if (not (CopSpawnTables::pursuitSpawnTable->IsInTable(assessment.copType)))
			{
				const float fleeTimestamp = *(this->simulationTime) + this->prng.GetFloat(maxFleeDelay, minFleeDelay);

				assessment.status   = Status::FLAGGED;
				assessment.position = this->waitingToFlee.insert({fleeTimestamp, assessment.copVehicle});

				if constexpr (Globals::loggingEnabled)
					Globals::Log(this->pursuit, "[FLE]", assessment.copVehicle, "to flee in", fleeTimestamp - (*this->simulationTime));
			}
		}


		void ReviewAll()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::Log(this->pursuit, "[FLE] Reviewing contingent");

			this->waitingToFlee.clear();

			for (auto& pair : this->copVehicleToAssessment)
			{
				if (pair.second.status != Status::FLEEING)
				{
					pair.second.status = Status::MEMBER;
					this->Review(pair.second);
				}
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
			auto        iterator = this->waitingToFlee.begin();

			while (fleeingEnabled and (iterator != this->waitingToFlee.end()))
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


		void ProcessAddition
		(
			const address                   copVehicle,
			const hash                      copType,
			const PursuitFeatures::CopLabel copLabel
		) 
			override
		{
			if (copLabel != PursuitFeatures::CopLabel::CHASER) return;

			this->Review(this->copVehicleToAssessment.insert({copVehicle, {copVehicle, copType, Status::MEMBER}}).first->second);
		}


		void ProcessRemoval
		(
			const address                   copVehicle,
			const address                   copType,
			const PursuitFeatures::CopLabel copLabel
		) 
			override
		{
			if (copLabel != PursuitFeatures::CopLabel::CHASER) return;

			const auto foundVehicle = this->copVehicleToAssessment.find(copVehicle);

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
	};





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		if (not parser.LoadFile(Globals::configAdvancedPath + "Cars.ini")) return;

		fleeingEnableds = parser.ParseParameterTable
		(
			"Fleeing:Timers",
			Globals::configFormat,
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(minFleeDelays, {}, 0.f),
			ConfigParser::FormatParameter<float, Globals::maxHeatLevel>(maxFleeDelays, {}, 0.f)
		);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			if (fleeingEnableds[heatLevel - 1]) featureEnabled = true;

			minFleeDelays[heatLevel - 1] = std::min(minFleeDelays[heatLevel - 1], maxFleeDelays[heatLevel - 1]);
		}
	}



	void SetToHeat(size_t heatLevel)
	{
		if (not featureEnabled)                                    return;
		if (not (fleeingEnabled = fleeingEnableds[heatLevel - 1])) return;

		minFleeDelay = minFleeDelays[heatLevel - 1];
		maxFleeDelay = maxFleeDelays[heatLevel - 1];
	}
}