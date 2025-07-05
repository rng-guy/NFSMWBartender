#pragma once

#include <Windows.h>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>
#include <array>

#include "Globals.h"
#include "ConfigParser.h"
#include "MemoryEditor.h"
#include "RandomNumbers.h"



namespace CopSpawnTables
{

	// SpawnTable class -----------------------------------------------------------------------------------------------------------------------------

	class SpawnTable
	{
	private:

		inline static RandomNumbers::Generator prng;

		struct Entry
		{
			int capacity;
			int chance;
		};

		int maxTotalCopCapacity = 0;
		int maxTotalCopChance   = 0;

		int availableTotalCopCapacity = 0;
		int availableTotalCopChance   = 0;

		std::unordered_set<hash>        availableCopTypes;
		std::unordered_map<hash, Entry> copTypeToEntry;

		inline static std::unordered_set<hash>              helicopterTypes;
		inline static std::unordered_map<std::string, hash> copNameToType;
		inline static std::unordered_map<hash, std::string> copTypeToName;
		


	public:

		static hash ConvertToType(const std::string& copName)
		{
			static hash (__cdecl* const GetType)(const char*) = (hash (__cdecl*)(const char*))0x5CC240;	

			const auto foundName = SpawnTable::copNameToType.find(copName);
			if (foundName != SpawnTable::copNameToType.end()) return foundName->second;

			const hash copType = GetType(copName.c_str());
			SpawnTable::copNameToType.insert({copName, copType});
			SpawnTable::copTypeToName.insert({copType, copName});

			return copType;
		}


		static const char* ConvertToName(const hash copType)
		{
			const auto foundType = SpawnTable::copTypeToName.find(copType);
			return (foundType != SpawnTable::copTypeToName.end()) ? (foundType->second).c_str() : nullptr;
		}


		static void RegisterHelicopter(const std::string& helicopterName)
		{
			SpawnTable::helicopterTypes.insert(SpawnTable::ConvertToType(helicopterName));
		}


		bool IsInTable(const hash copType) const
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
			
			const hash copType = this->ConvertToType(copName);
			if (this->helicopterTypes.contains(copType)) return false;
			if (this->IsInTable(copType))                return false;

			this->maxTotalCopCapacity       += copCount;
			this->maxTotalCopChance         += copChance;
			this->availableTotalCopCapacity += copCount;
			this->availableTotalCopChance   += copChance;

			this->availableCopTypes.insert(copType);
			this->copTypeToEntry.insert({copType, {copCount, copChance}});

			return true;
		}


		int GetCapacity(const hash copType) const
		{
			return (this->IsInTable(copType)) ? copTypeToEntry.at(copType).capacity : 0;
		}


		bool UpdateCapacity
		(
			const hash copType,
			const int  change
		) {
			if (not this->IsInTable(copType)) return false;

			Entry&     typeEntry    = copTypeToEntry.at(copType);
			const bool wasAvailable = (typeEntry.capacity > 0);

			typeEntry.capacity              += change;
			this->availableTotalCopCapacity += change;

			if (wasAvailable xor (typeEntry.capacity > 0))
			{
				if (wasAvailable)
				{
					this->availableTotalCopChance -= typeEntry.chance;
					this->availableCopTypes.erase(copType);
				}
				else
				{
					this->availableTotalCopChance += typeEntry.chance;
					this->availableCopTypes.insert(copType);
				}
			}

			return true;
		}


		bool IsEmpty() const {return (this->availableTotalCopCapacity < 1);}


		const char* GetRandomCopName() const
		{
			if (this->IsEmpty()) return nullptr;

			const int randomNumber     = this->prng.GetInt(this->availableTotalCopChance);
			int       cumulativeChance = 0;

			for (const hash copType : this->availableCopTypes)
			{
				cumulativeChance += this->copTypeToEntry.at(copType).chance;
				if (randomNumber < cumulativeChance) return this->ConvertToName(copType);
			}

			if constexpr (Globals::loggingEnabled)
				Globals::Log("WARNING: [TAB] Failed to select vehicle:", randomNumber, cumulativeChance, this->availableTotalCopChance);

			return nullptr;
		}


		int GetMaxTotalCopCapacity()       const {return this->maxTotalCopCapacity;}
		int GetMaxTotalCopChance()         const {return this->maxTotalCopChance;}
		int GetAvailableTotalCopCapacity() const {return this->availableTotalCopCapacity;}
		int GetAvailableTotalCopChance()   const {return this->availableTotalCopChance;}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Current Heat level
	const char*       helicopterVehicle   = "copheli";
	const SpawnTable* eventSpawnTable     = nullptr;
	const SpawnTable* patrolSpawnTable    = nullptr;
	const SpawnTable* pursuitSpawnTable   = nullptr;
	const SpawnTable* roadblockSpawnTable = nullptr;

	// Free-roam Heat levels
	std::array<std::string, Globals::maxHeatLevel> roamHelicopterVehicles   = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> roamEventSpawnTables     = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> roamPatrolSpawnTables    = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> roamPursuitSpawnTables   = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> roamRoadblockSpawnTables = {};

	// Racing Heat levels
	std::array<std::string, Globals::maxHeatLevel> raceHelicopterVehicles   = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> raceEventSpawnTables     = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> racePatrolSpawnTables    = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> racePursuitSpawnTables   = {};
	std::array<SpawnTable,  Globals::maxHeatLevel> raceRoadblockSpawnTables = {};

	// Code caves
	int currentMaxCopCapacity = 0;





	// Auxiliary functinos --------------------------------------------------------------------------------------------------------------------------

	void ParseTables
	(
		ConfigParser::Parser&                          parser,
		const std::string&                             name,
		const std::string&                             format,
		std::array<SpawnTable, Globals::maxHeatLevel>& spawnTables,
		const bool                                     hasCounts
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
					ConfigParser::UserParameter<int>(copCounts, 1), 
					ConfigParser::UserParameter<int>(copChances, 1)
				);

			else 
				numberOfEntries = parser.ParseUserParameter<int>(section, copNames, copChances, 1);

			for (size_t entryID = 0; entryID < numberOfEntries; entryID++)
				spawnTables[heatLevel - 1].AddType(copNames[entryID], (hasCounts) ? copCounts[entryID] : 1, copChances[entryID]);
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	void Initialise(ConfigParser::Parser& parser)
	{
		parser.LoadFile(Globals::configPathAdvanced + "Helicopter.ini");

		// Pursuit parameters
		Globals::ParseHeatParameter<std::string>
		(
			parser,
			"Helicopter:Vehicle",
			roamHelicopterVehicles,
			raceHelicopterVehicles,
			helicopterVehicle
		);

		std::for_each(roamHelicopterVehicles.begin(), roamHelicopterVehicles.end(), SpawnTable::RegisterHelicopter);
		std::for_each(raceHelicopterVehicles.begin(), raceHelicopterVehicles.end(), SpawnTable::RegisterHelicopter);

		// Free-roam spawn tables
		if (not parser.LoadFile(Globals::configPathAdvanced + "Cars.ini")) return;

		const std::string formatRoam = "Heat{:02}:";

		ParseTables(parser, "Chasers", formatRoam, roamPursuitSpawnTables, true);
		for (const auto& spawnTable : roamPursuitSpawnTables) if (spawnTable.IsEmpty()) return;

		ParseTables(parser, "Events",     formatRoam, roamEventSpawnTables,     true);
		ParseTables(parser, "Patrols",    formatRoam, roamPatrolSpawnTables,    false);
		ParseTables(parser, "Roadblocks", formatRoam, roamRoadblockSpawnTables, true);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			if (roamEventSpawnTables[heatLevel - 1].IsEmpty())     roamEventSpawnTables[heatLevel - 1]     = roamPursuitSpawnTables[heatLevel - 1];
			if (roamPatrolSpawnTables[heatLevel - 1].IsEmpty())    roamPatrolSpawnTables[heatLevel - 1]    = roamPursuitSpawnTables[heatLevel - 1];
			if (roamRoadblockSpawnTables[heatLevel - 1].IsEmpty()) roamRoadblockSpawnTables[heatLevel - 1] = roamPursuitSpawnTables[heatLevel - 1];
		}

		// Racing spawn tables
		const std::string formatRace = "Race{:02}:";

		ParseTables(parser, "Chasers",    formatRace, racePursuitSpawnTables,   true);
		ParseTables(parser, "Events",     formatRace, raceEventSpawnTables,     true);
		ParseTables(parser, "Patrols",    formatRace, racePatrolSpawnTables,    false);
		ParseTables(parser, "Roadblocks", formatRace, raceRoadblockSpawnTables, true);

		for (size_t heatLevel = 1; heatLevel <= Globals::maxHeatLevel; heatLevel++)
		{
			if (racePursuitSpawnTables[heatLevel - 1].IsEmpty())   racePursuitSpawnTables[heatLevel - 1]   = roamPursuitSpawnTables[heatLevel - 1];
			if (raceEventSpawnTables[heatLevel - 1].IsEmpty())     raceEventSpawnTables[heatLevel - 1]     = roamEventSpawnTables[heatLevel - 1];
			if (racePatrolSpawnTables[heatLevel - 1].IsEmpty())    racePatrolSpawnTables[heatLevel - 1]    = roamPatrolSpawnTables[heatLevel - 1];
			if (raceRoadblockSpawnTables[heatLevel - 1].IsEmpty()) raceRoadblockSpawnTables[heatLevel - 1] = roamRoadblockSpawnTables[heatLevel - 1];
		}

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
			helicopterVehicle = raceHelicopterVehicles[heatLevel - 1].c_str();

			eventSpawnTable     = &raceEventSpawnTables[heatLevel - 1];
			patrolSpawnTable    = &racePatrolSpawnTables[heatLevel - 1];
			pursuitSpawnTable   = &racePursuitSpawnTables[heatLevel - 1];
			roadblockSpawnTable = &raceRoadblockSpawnTables[heatLevel - 1];
		}
		else
		{
			helicopterVehicle = roamHelicopterVehicles[heatLevel - 1].c_str();

			eventSpawnTable     = &roamEventSpawnTables[heatLevel - 1];
			patrolSpawnTable    = &roamPatrolSpawnTables[heatLevel - 1];
			pursuitSpawnTable   = &roamPursuitSpawnTables[heatLevel - 1];
			roadblockSpawnTable = &roamRoadblockSpawnTables[heatLevel - 1];
		}

		currentMaxCopCapacity = pursuitSpawnTable->GetMaxTotalCopCapacity();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::LogIndent("[TAB] CopSpawnTables");

			Globals::LogLongIndent("helicopterVehicle      :", helicopterVehicle);
		}
	}
}