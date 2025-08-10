#pragma once

#include <algorithm>
#include <string>
#include <format>
#include <vector>

#include "Globals.h"
#include "MemoryEditor.h"
#include "HeatParameters.h"



namespace CopSpawnTables
{

	// SpawnTable class -----------------------------------------------------------------------------------------------------------------------------

	class SpawnTable
	{
	private:

		struct Entry
		{
			int capacity;
			int chance;
		};

		int maxTotalCopCapacity   = 0;
		int maxTotalCopChance     = 0;
		int currentTotalCopChance = 0;

		Globals::HashMap<Entry> copTypeToEntry;

		inline static Globals::HashSet              helicopterTypes;
		inline static Globals::HashMap<std::string> copTypeToName;
		


	public:

		static hash NameToType(const std::string& copName)
		{
			const hash copType = Globals::GetStringHash(copName.c_str());
			SpawnTable::copTypeToName.insert({copType, copName});
			return copType;
		}


		static const char* TypeToName(const hash copType)
		{
			const auto foundType = SpawnTable::copTypeToName.find(copType);
			return (foundType != SpawnTable::copTypeToName.end()) ? (foundType->second).c_str() : nullptr;
		}


		static void RegisterHelicopter(const std::string& helicopterName)
		{
			SpawnTable::helicopterTypes.insert(SpawnTable::NameToType(helicopterName));
		}


		bool Contains(const hash copType) const
		{
			return this->copTypeToEntry.contains(copType);
		}


		bool AddType
		(
			const std::string& copName,
			const int          copCount, 
			const int          copChance
		) {
			if (copCount < 1)  return false;
			if (copChance < 1) return false;
			
			const hash copType = this->NameToType(copName);
			if (this->helicopterTypes.contains(copType)) return false;

			if (this->copTypeToEntry.insert({copType, {copCount, copChance}}).second)
			{
				this->maxTotalCopCapacity   += copCount;
				this->maxTotalCopChance     += copChance;
				this->currentTotalCopChance += copChance;

				return true;
			}
			
			return false;
		}


		void RemoveInvalidVehicles()
		{
			auto iterator = this->copTypeToEntry.begin();

			while (iterator != this->copTypeToEntry.end())
			{
				if (not HeatParameters::IsValidVehicle(iterator->first))
				{
					this->maxTotalCopCapacity -= iterator->second.capacity;
					this->maxTotalCopChance   -= iterator->second.chance;

					if (iterator->second.capacity > 0)
						this->currentTotalCopChance -= iterator->second.chance;

					this->copTypeToEntry.erase(iterator++);
				}
				else iterator++;
			}

			if (this->IsEmpty()) this->AddType("copmidsize", 1, 1);
		}


		int GetCapacity(const hash copType) const
		{
			const auto foundType = this->copTypeToEntry.find(copType);
			return (foundType != this->copTypeToEntry.end()) ? foundType->second.capacity : 0;
		}


		bool UpdateCapacity
		(
			const hash copType,
			const int  change
		) {
			const auto foundType = this->copTypeToEntry.find(copType);
			if (foundType == this->copTypeToEntry.end()) return false;

			const bool wasAvailable = (foundType->second.capacity > 0);

			foundType->second.capacity += change;

			if (wasAvailable xor (foundType->second.capacity > 0))
			{
				if (wasAvailable)
					this->currentTotalCopChance -= foundType->second.chance;
				
				else
					this->currentTotalCopChance += foundType->second.chance;
			}

			return true;
		}


		bool IsEmpty() const {return (this->maxTotalCopCapacity < 1);}


		const char* GetRandomCopName() const
		{
			if (this->currentTotalCopChance < 1) return nullptr;

			const int randomNumber     = Globals::prng.Generate<int>(0, this->currentTotalCopChance);
			int       cumulativeChance = 0;

			for (const auto& pair : this->copTypeToEntry)
			{
				if (pair.second.capacity > 0)
				{
					cumulativeChance += pair.second.chance;

					if (cumulativeChance > randomNumber) 
						return this->TypeToName(pair.first);
				}
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [TAB] Failed to select vehicle:", randomNumber, cumulativeChance, this->currentTotalCopChance);

			return nullptr;
		}


		int GetMaxTotalCopCapacity() const {return this->maxTotalCopCapacity;}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<std::string> helicopterVehicles("copheli");
	HeatParameters::Pair<SpawnTable>  eventSpawnTables;
	HeatParameters::Pair<SpawnTable>  patrolSpawnTables;
	HeatParameters::Pair<SpawnTable>  pursuitSpawnTables;
	HeatParameters::Pair<SpawnTable>  roadblockSpawnTables;

	// Code caves
	int currentMaxCopCapacity = 0;





	// Auxiliary functinos --------------------------------------------------------------------------------------------------------------------------

	void ParseTables
	(
		HeatParameters::Parser&             parser,
		const std::string&                  name,
		const std::string&                  format,
		HeatParameters::Values<SpawnTable>& spawnTables,
		const bool                          hasCounts
	) {
		std::string section;

		std::vector<std::string> copNames;
		std::vector<int>         copCounts;
		std::vector<int>         copChances;

		size_t numberOfEntries;

		for (size_t heatLevel = 1; heatLevel <= HeatParameters::maxHeatLevel; heatLevel++)
		{
			section = std::vformat(format + name, std::make_format_args(heatLevel));

			if (hasCounts) 
				numberOfEntries = parser.ParseParameterTable
				(
					section,
					copNames, 
					ConfigParser::UserParameter<int>(copCounts,  0), 
					ConfigParser::UserParameter<int>(copChances, 0)
				);

			else 
				numberOfEntries = parser.ParseUserParameter<int>(section, copNames, copChances, 0);

			for (size_t entryID = 0; entryID < numberOfEntries; entryID++)
				spawnTables[heatLevel - 1].AddType(copNames[entryID], (hasCounts) ? copCounts[entryID] : 1, copChances[entryID]);
		}
	}



	void ReplaceEmptyTables
	(
		HeatParameters::Values<SpawnTable>&       tables,
		const HeatParameters::Values<SpawnTable>& replacements
	) {
		for (size_t heatLevel = 1; heatLevel <= HeatParameters::maxHeatLevel; heatLevel++)
			if (tables[heatLevel - 1].IsEmpty()) tables[heatLevel - 1] = replacements[heatLevel - 1];
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Helicopter.ini");

		// Pursuit parameters
		HeatParameters::Parse<std::string>(parser, "Helicopter:Vehicle", {helicopterVehicles});
		std::for_each(helicopterVehicles.roam.begin(), helicopterVehicles.roam.end(), SpawnTable::RegisterHelicopter);
		std::for_each(helicopterVehicles.race.begin(), helicopterVehicles.race.end(), SpawnTable::RegisterHelicopter);

		// Spawn tables
		if (not parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini")) return false;

		for (const bool forRaces : {false, true})
		{
			const std::string format = (forRaces) ? "Race{:02}:" : "Heat{:02}:";

			ParseTables(parser, "Chasers",    format, pursuitSpawnTables.Get  (forRaces), true);
			ParseTables(parser, "Events",     format, eventSpawnTables.Get    (forRaces), true);
			ParseTables(parser, "Patrols",    format, patrolSpawnTables.Get   (forRaces), false);
			ParseTables(parser, "Roadblocks", format, roadblockSpawnTables.Get(forRaces), true);
		}

		// Validation
		for (const SpawnTable& table : pursuitSpawnTables.roam) if (table.IsEmpty()) return false;

		ReplaceEmptyTables(eventSpawnTables.roam,     pursuitSpawnTables.roam);
		ReplaceEmptyTables(patrolSpawnTables.roam,    pursuitSpawnTables.roam);
		ReplaceEmptyTables(roadblockSpawnTables.roam, pursuitSpawnTables.roam);

		ReplaceEmptyTables(pursuitSpawnTables.race,   pursuitSpawnTables.roam);
		ReplaceEmptyTables(eventSpawnTables.race,     eventSpawnTables.roam);
		ReplaceEmptyTables(patrolSpawnTables.race,    patrolSpawnTables.roam);
		ReplaceEmptyTables(roadblockSpawnTables.race, roadblockSpawnTables.roam);

		return (featureEnabled = true);
	}



	void Validate()
	{
		if (not featureEnabled) return;

		HeatParameters::ReplaceInvalidVehicles(helicopterVehicles);

		for (const bool forRaces : {false, true})
		{
			for (SpawnTable& table : eventSpawnTables.Get    (forRaces)) table.RemoveInvalidVehicles();
			for (SpawnTable& table : patrolSpawnTables.Get   (forRaces)) table.RemoveInvalidVehicles();
			for (SpawnTable& table : pursuitSpawnTables.Get  (forRaces)) table.RemoveInvalidVehicles();
			for (SpawnTable& table : roadblockSpawnTables.Get(forRaces)) table.RemoveInvalidVehicles();
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		helicopterVehicles.SetToHeat  (isRacing, heatLevel);
		eventSpawnTables.SetToHeat    (isRacing, heatLevel);
		patrolSpawnTables.SetToHeat   (isRacing, heatLevel);
		pursuitSpawnTables.SetToHeat  (isRacing, heatLevel);
		roadblockSpawnTables.SetToHeat(isRacing, heatLevel);

		currentMaxCopCapacity = pursuitSpawnTables.current->GetMaxTotalCopCapacity();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.LogIndent("[TAB] CopSpawnTables");

			Globals::logger.LogLongIndent("helicopterVehicle      :", helicopterVehicles.current);
		}
	}
}