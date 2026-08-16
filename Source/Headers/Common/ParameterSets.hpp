#pragma once

#include <vector>
#include <limits>
#include <string_view>

#include "Globals.hpp"
#include "ModContainers.hpp"
#include "HeatParameters.hpp"



namespace ParameterSets
{
	// CopInteractions class ------------------------------------------------------------------------------------------------------------------------

	class CopInteractions
	{
	private: // members

		// Heat parameters
		HEAT_PARAMETER_VALUE(float, copTagChange, {0.f});

		HEAT_PARAMETER_VALUE(float, changePerAssault, {0.f});

		OPTIONAL_HEAT_PARAMETER_VALUE(int, maxNumAssaultsPerCop, {0, std::numeric_limits<byte>::max() - 1});

		HEAT_PARAMETER_VALUE(float, copWreckChange, {0.f});

		// Vehicle maps
		DEFAULT_VAULT_MAP(float, copTypeToTagChange, 0.f);

		DEFAULT_VAULT_MAP(float, copTypeToAssaultChange, 0.f);

		DEFAULT_VAULT_MAP(float, copTypeToWreckChange, 0.f);


	private: // methods

		static void ParseVehicleMap
		(
			const HeatParameters::Parser&          parser,
			const std::string_view                 section,
			ModContainers::DefaultVaultMap<float>& vehicleMap
		) {
			std::vector<std::string_view> copNames;
			std::vector<float>            changes;

			parser.ParseUser<std::string_view, float>(section, copNames, {changes});

			vehicleMap.Fill
			(
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
			HeatParameters::Parse(parser, HeatParameters::buffer.Format("{}:Tagging", featureTag), this->copTagChange);

			HeatParameters::Parse(parser, HeatParameters::buffer.Format("{}:Assault", featureTag), this->changePerAssault);

			HeatParameters::Parse(parser, "Assault:Limit", this->maxNumAssaultsPerCop);

			HeatParameters::Parse(parser, HeatParameters::buffer.Format("{}:Wrecking", featureTag), this->copWreckChange);

			// Vehicle-to-change maps
			this->ParseVehicleMap(parser, "Tagging:Vehicles", this->copTypeToTagChange);

			this->ParseVehicleMap(parser, "Assault:Vehicles", this->copTypeToAssaultChange);

			this->ParseVehicleMap(parser, "Wrecking:Vehicles", this->copTypeToWreckChange);
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


		void SetToHeatState(const HeatParameters::HeatState state)
		{
			this->copTagChange.SetToHeatState(state);

			this->changePerAssault.SetToHeatState(state);

			this->maxNumAssaultsPerCop.SetToHeatState(state);

			this->copWreckChange.SetToHeatState(state);
		}
	};
}