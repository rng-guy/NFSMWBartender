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

		HashContainers::AddressSet        activeChaserVehicles;
		HashContainers::AddressMap<float> copVehicleToFleeTimestamp;
		
		
		static bool IsVehicleTrackable(const CopLabel copLabel)
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


		static bool IsChaserMember(const address copVehicle)
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return CopSpawnTables::chaserSpawnTables.current->Contains(copType);
		}


		void ReviewVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) {
			if (not PursuitFeatures::heatLevelKnown) return;

			bool  flagVehicle   = false;
			float timeUntilFlee = 0.f;

			switch (copLabel)
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
				flagVehicle = (fleeingEnableds.current and (not this->IsChaserMember(copVehicle)));
				if (flagVehicle) timeUntilFlee = Globals::prng.GenerateNumber<float>(minFleeDelays.current, maxFleeDelays.current);
			}

			if (flagVehicle)
			{
				this->copVehicleToFleeTimestamp.insert({copVehicle, PursuitFeatures::simulationTime + timeUntilFlee});

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[FLE]", copVehicle, "timer expiring", timeUntilFlee);
			}
		}


		void ReviewChasers()
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[FLE] Reviewing active chasers");

			for (address chaserVehicle : this->activeChaserVehicles)
			{
				this->copVehicleToFleeTimestamp.erase(chaserVehicle);
				this->ReviewVehicle(chaserVehicle, CopLabel::CHASER);
			}
		}


		void CheckFleeTimestamps()
		{
			if (this->copVehicleToFleeTimestamp.empty()) return;
			
			auto iterator = this->copVehicleToFleeTimestamp.begin();

			while (iterator != this->copVehicleToFleeTimestamp.end())
			{
				if (PursuitFeatures::simulationTime >= iterator->second)
				{
					PursuitFeatures::MakeVehicleFlee(iterator->first);
					this->activeChaserVehicles.erase(iterator->first);

					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log(this->pursuit, "[FLE]", iterator->first, "timer expired");

					iterator = this->copVehicleToFleeTimestamp.erase(iterator);
				}
				else ++iterator;
			}
		}



	public:

		explicit MembershipManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) {}


		void UpdateOnGameplay() override 
		{
			this->CheckFleeTimestamps();
		}


		void UpdateOnHeatChange() override 
		{
			this->ReviewChasers();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		) 
			override
		{
			if (not this->IsVehicleTrackable(copLabel)) return;

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
			if (not this->IsVehicleTrackable(copLabel)) return;

			if (copLabel == CopLabel::CHASER)
			{
				if (this->activeChaserVehicles.erase(copVehicle))
					this->copVehicleToFleeTimestamp.erase(copVehicle);
			}
			else this->copVehicleToFleeTimestamp.erase(copVehicle);
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