#pragma once

#include <vector>
#include <limits>
#include <format>
#include <string_view>

#include "Globals.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace ParameterSets
{
	// CopInteractions class ------------------------------------------------------------------------------------------------------------------------

	class CopInteractions
	{
	private: // members

		// Heat parameters
		HeatParameters::Value<float> copTagChange{0.f};

		HeatParameters::Value<float> changePerAssault{0.f};

		HeatParameters::OptionalValue<int> maxNumAssaultsPerCop{{0, std::numeric_limits<byte>::max() - 1}};

		HeatParameters::Value<float> copWreckChange{0.f};

		// Vehicle maps
		ModContainers::DefaultVaultMap<float> copTypeToTagChange{0.f};

		ModContainers::DefaultVaultMap<float> copTypeToAssaultChange{0.f};

		ModContainers::DefaultVaultMap<float> copTypeToWreckChange{0.f};


	private: // methods

		static void ParseVehicleMap
		(
			const HeatParameters::Parser&          parser,
			const std::string_view                 section,
			ModContainers::DefaultVaultMap<float>& vehicleMap,
			const std::string_view                 mapName
		) {
			std::vector<std::string_view> copNames;
			std::vector<float>            changes;

			parser.ParseUser<std::string_view, float>(section, copNames, {changes});

			vehicleMap.Fill
			(
				mapName,
				HeatParameters::configDefaultKey,
				ModContainers::FillSetup(copNames, Globals::GetVaultHash,         Globals::DoesVehicleTypeExist),
				ModContainers::FillSetup(changes,  ModContainers::IdentityCopy(), ModContainers::AlwaysValid())
			);
		}


		bool GetsCreditForAssault(const byte numCopAssaulted) const
		{
			if (not maxNumAssaultsPerCop.isEnabled.current) return true;
			return (numCopAssaulted < maxNumAssaultsPerCop.value.current);
		}


	public: // methods

		void Parse
		(
			const HeatParameters::Parser& parser,
			const std::string_view        featureTag
		) {
			// Heat parameters
			HeatParameters::Parse(parser, std::format("{}:Tagging", featureTag), this->copTagChange);

			HeatParameters::Parse(parser, std::format("{}:Assault", featureTag), this->changePerAssault);

			HeatParameters::Parse(parser, "Assault:Limit", this->maxNumAssaultsPerCop);

			HeatParameters::Parse(parser, std::format("{}:Wrecking", featureTag), this->copWreckChange);

			// Vehicle-to-change maps
			this->ParseVehicleMap(parser, "Tagging:Vehicles", this->copTypeToTagChange, "Vehicle-to-tag");

			this->ParseVehicleMap(parser, "Assault:Vehicles", this->copTypeToAssaultChange, "Vehicle-to-assault");

			this->ParseVehicleMap(parser, "Wrecking:Vehicles", this->copTypeToWreckChange, "Vehicle-to-wreck");
		}


		float GetTaggingChange(const address copVehicle) const
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return this->copTagChange.current + this->copTypeToTagChange.GetValue(copType);
		}


		float GetAssaultChange
		(
			const address copVehicle,
			const byte    numCopAssaulted
		)
			const
		{
			if (not this->GetsCreditForAssault(numCopAssaulted)) return 0.f;

			const vault copType = Globals::GetVehicleType(copVehicle);
			return this->changePerAssault.current + this->copTypeToAssaultChange.GetValue(copType);
		}


		float GetWreckingChange(const address copVehicle) const
		{
			const vault copType = Globals::GetVehicleType(copVehicle);
			return this->copWreckChange.current + this->copTypeToWreckChange.GetValue(copType);
		}


		void Log
		(
			const std::string_view tagChangeName,
			const std::string_view perAssaultName,
			const std::string_view maxNumAssaultsName,
			const std::string_view wreckChangeName
		)
			const
		{
			this->copTagChange.Log(tagChangeName);

			this->changePerAssault.Log(perAssaultName);

			this->maxNumAssaultsPerCop.Log(maxNumAssaultsName);

			this->copWreckChange.Log(wreckChangeName);
		}


		void SetToHeatState(const HeatParameters::HeatState state)
		{
			this->copTagChange.SetToHeatState(state);

			this->changePerAssault.SetToHeatState(state);

			this->maxNumAssaultsPerCop.SetToHeatState(state);

			this->copWreckChange.SetToHeatState(state);
		}
	};
}