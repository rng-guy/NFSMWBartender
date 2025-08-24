#pragma once

#include <initializer_list>

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

	class MembershipManager : public PursuitFeatures::PursuitReaction
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

		float heavyStrategyDuration  = 0.f;
		float leaderStrategyDuration = 0.f;

		Globals::AddressMap<Assessment> copVehicleToAssessment;
		Globals::AddressMap<float>      copVehicleToFleeTimestamp;
		
		
		static float GetStrategyDuration
		(
			const address                     supportNode,
			const address                     getStrategy,
			const address                     getNumStrategies,
			const std::initializer_list<int>& validStrategyTypes
		) {
			size_t  (__thiscall* const GetNumStrategies)(address)         = (size_t  (__thiscall*)(address))        getNumStrategies;
			address (__thiscall* const GetStrategy)     (address, size_t) = (address (__thiscall*)(address, size_t))getStrategy;

			const size_t numStrategies = GetNumStrategies(supportNode);

			for (size_t strategyID = 0; strategyID < numStrategies; strategyID++)
			{
				const address strategy     = GetStrategy(supportNode, strategyID);
				const int     strategyType = *((int*)strategy);

				for (const int validStrategyType : validStrategyTypes)
					if (strategyType == validStrategyType) return *((float*)(strategy + 0x8));
			}

			return 0.f;
		}


		void UpdateStrategyDurations()
		{
			// PursuitSupport node
			static address (__thiscall* const GetSupportNode)(address) = (address (__thiscall*)(address))0x418EE0;

			const address supportNode = GetSupportNode(this->pursuit - 0x48);

			if (not supportNode)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [FLE] Invalid SupportNode pointer in", this->pursuit);

				this->heavyStrategyDuration  = 0.f;
				this->leaderStrategyDuration = 0.f;

				return;
			}

			// HeavyStrategy 3
			this->heavyStrategyDuration = this->GetStrategyDuration(supportNode, 0x4035E0, 0x403600, {3});

			if constexpr (Globals::loggingEnabled)
			{
				if (this->heavyStrategyDuration > 0.f)
					Globals::logger.Log(this->pursuit, "[FLE] HeavyStrategy 3 duration:", this->heavyStrategyDuration);
			}

			// LeaderStrategies
			this->leaderStrategyDuration = this->GetStrategyDuration(supportNode, 0x403660, 0x403680, {5, 7});
			
			if constexpr (Globals::loggingEnabled)
			{
				if (this->leaderStrategyDuration > 0.f)
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


		void CheckTimestamps()
		{
			if (this->copVehicleToFleeTimestamp.empty()) return;
			
			const float simulationTime = *(this->simulationTime);
			auto        iterator       = this->copVehicleToFleeTimestamp.begin();

			while (iterator != this->copVehicleToFleeTimestamp.end())
			{
				if (simulationTime >= iterator->second)
				{
					this->copVehicleToAssessment.at(iterator->first).status = Status::FLEEING;
					this->MakeFlee(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE]", iterator->first, "now fleeing");

					iterator = this->copVehicleToFleeTimestamp.erase(iterator);
				}
				else iterator++;
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

		// Code caves
		MemoryEditor::DigCodeCave(UpdateFormation, updateFormationEntrance, updateFormationExit);

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