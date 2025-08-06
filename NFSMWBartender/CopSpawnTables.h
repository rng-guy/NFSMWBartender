#pragma once

#include <Windows.h>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <format>
#include <vector>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"



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

		std::unordered_map<hash, Entry, Globals::IdentityHash> copTypeToEntry;

		inline static std::unordered_set<hash, Globals::IdentityHash>              helicopterTypes;
		inline static std::unordered_map<hash, std::string, Globals::IdentityHash> copTypeToName;
		


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
				Globals::Log("WARNING: [TAB] Failed to select vehicle:", randomNumber, cumulativeChance, this->currentTotalCopChance);

			return nullptr;
		}


		int GetMaxTotalCopCapacity() const {return this->maxTotalCopCapacity;}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	const char*       helicopterVehicle   = "copheli";
	const SpawnTable* eventSpawnTable     = nullptr;
	const SpawnTable* patrolSpawnTable    = nullptr;
	const SpawnTable* pursuitSpawnTable   = nullptr;
	const SpawnTable* roadblockSpawnTable = nullptr;

	// Heat levels
	Globals::HeatParametersPair<std::string> helicopterVehicles;
	Globals::HeatParametersPair<SpawnTable>  eventSpawnTables;
	Globals::HeatParametersPair<SpawnTable>  patrolSpawnTables;
	Globals::HeatParametersPair<SpawnTable>  pursuitSpawnTables;
	Globals::HeatParametersPair<SpawnTable>  roadblockSpawnTables;

	// Code caves
	int currentMaxCopCapacity = 0;





	// Auxiliary functinos --------------------------------------------------------------------------------------------------------------------------

	void ParseTables
	(
		ConfigParser::Parser&                parser,
		const std::string&                   name,
		const std::string&                   format,
		Globals::HeatParameters<SpawnTable>& spawnTables,
		const bool                           hasCounts
	) {
		std::string section;

		std::vector<std::string> copNames;
		std::vector<int>         copCounts;
		std::vector<int>         copChances;

		size_t numberOfEntries;

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			section = std::vformat(format + name, std::make_format_args(heatLevel));

			if (hasCounts) 
				numberOfEntries = parser.ParseParameterTable
				(
					section,
					copNames, 
					ConfigParser::UserParameter<int>(copCounts,  1), 
					ConfigParser::UserParameter<int>(copChances, 1)
				);

			else 
				numberOfEntries = parser.ParseUserParameter<int>(section, copNames, copChances, 1);

			for (size_t entryID = 0; entryID < numberOfEntries; entryID++)
				spawnTables[heatLevel - 1].AddType(copNames[entryID], (hasCounts) ? copCounts[entryID] : 1, copChances[entryID]);
		}
	}



	void ValidateTables
	(
		Globals::HeatParameters<SpawnTable>&       tables,
		const Globals::HeatParameters<SpawnTable>& fallbacks
	) {
		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
			if (tables[heatLevel - 1].IsEmpty()) tables[heatLevel - 1] = fallbacks[heatLevel - 1];
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		parser.LoadFile(Globals::configPathAdvanced + "Helicopter.ini");

		// Pursuit parameters
		Globals::ParseHeatParameters<std::string>
		(
			parser,
			"Helicopter:Vehicle",
			{helicopterVehicles, helicopterVehicle}
		);

		std::for_each(helicopterVehicles.roam.begin(), helicopterVehicles.roam.end(), SpawnTable::RegisterHelicopter);
		std::for_each(helicopterVehicles.race.begin(), helicopterVehicles.race.end(), SpawnTable::RegisterHelicopter);

		// Spawn tables
		if (not parser.LoadFile(Globals::configPathAdvanced + "Cars.ini")) return false;

		for (const bool forRaces : {false, true})
		{
			const std::string format = (forRaces) ? "Race{:02}:" : "Heat{:02}:";

			ParseTables(parser, "Chasers",    format, pursuitSpawnTables  (forRaces), true);
			ParseTables(parser, "Events",     format, eventSpawnTables    (forRaces), true);
			ParseTables(parser, "Patrols",    format, patrolSpawnTables   (forRaces), false);
			ParseTables(parser, "Roadblocks", format, roadblockSpawnTables(forRaces), true);
		}

		// Validation
		for (const SpawnTable& table : pursuitSpawnTables.roam) if (table.IsEmpty()) return false;

		ValidateTables(eventSpawnTables.roam,     pursuitSpawnTables.roam);
		ValidateTables(patrolSpawnTables.roam,    pursuitSpawnTables.roam);
		ValidateTables(roadblockSpawnTables.roam, pursuitSpawnTables.roam);

		ValidateTables(pursuitSpawnTables.race,   pursuitSpawnTables.roam);
		ValidateTables(eventSpawnTables.race,     eventSpawnTables.roam);
		ValidateTables(patrolSpawnTables.race,    patrolSpawnTables.roam);
		ValidateTables(roadblockSpawnTables.race, roadblockSpawnTables.roam);

		return (featureEnabled = true);
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		helicopterVehicle   = helicopterVehicles    (isRacing, heatLevel).c_str();
		eventSpawnTable     = &(eventSpawnTables    (isRacing, heatLevel));
		patrolSpawnTable    = &(patrolSpawnTables   (isRacing, heatLevel));
		pursuitSpawnTable   = &(pursuitSpawnTables  (isRacing, heatLevel));
		roadblockSpawnTable = &(roadblockSpawnTables(isRacing, heatLevel));

		currentMaxCopCapacity = pursuitSpawnTable->GetMaxTotalCopCapacity();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[TAB] CopSpawnTables");

			Globals::LogLongIndent("helicopterVehicle      :", helicopterVehicle);
		}
	}
}