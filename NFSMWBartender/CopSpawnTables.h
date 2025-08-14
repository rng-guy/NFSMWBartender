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

		Globals::VaultMap<Entry> copTypeToEntry;

		inline static Globals::VaultMap<std::string> copTypeToName;
		


	public:

		static vault NameToType(const std::string& copName)
		{
			const vault copType = Globals::GetVaultKey(copName.c_str());
			SpawnTable::copTypeToName.insert({copType, copName});
			return copType;
		}


		static const char* TypeToName(const vault copType)
		{
			const auto foundType = SpawnTable::copTypeToName.find(copType);
			return (foundType != SpawnTable::copTypeToName.end()) ? (foundType->second).c_str() : nullptr;
		}


		bool Contains(const vault copType) const
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
			
			const vault copType   = this->NameToType(copName);
			const auto  addedType = this->copTypeToEntry.insert({copType, {copCount, copChance}});

			if (not addedType.second) return false;

			this->maxTotalCopCapacity   += copCount;
			this->maxTotalCopChance     += copChance;
			this->currentTotalCopChance += copChance;

			return true;
		}


		bool IsEmpty() const {return (this->maxTotalCopCapacity < 1);}


		size_t RemoveInvalidTypes
		(
			const char* const tableName, 
			const bool        forRaces, 
			const size_t      heatLevel
		) {
			auto   iterator   = this->copTypeToEntry.begin();
			size_t numRemoved = 0;

			while (iterator != this->copTypeToEntry.end())
			{
				if (not HeatParameters::VehicleClassMatches(iterator->first, false))
				{
					// With logging disabled, the compiler optimises all parameters away
					if constexpr (Globals::loggingEnabled)
					{
						if (numRemoved == 0)
							Globals::logger.LogLongIndent(tableName, (int)heatLevel, (forRaces) ? "(race)" : "(free-roam)");
						
						Globals::logger.LogLongIndent
						(
							"  -", 
							this->TypeToName(iterator->first), 
							iterator->second.capacity, 
							iterator->second.chance
						);
					}

					this->maxTotalCopCapacity -= iterator->second.capacity;
					this->maxTotalCopChance   -= iterator->second.chance;

					if (iterator->second.capacity > 0)
						this->currentTotalCopChance -= iterator->second.chance;

					this->copTypeToEntry.erase(iterator++);

					numRemoved++;
				}
				else iterator++;
			}

			static const std::string fillerVehicle = "copmidsize";

			if (this->IsEmpty())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.LogLongIndent("  +", fillerVehicle, 1, 100);

				this->AddType(fillerVehicle, 1, 100);
			}

			if constexpr (Globals::loggingEnabled)
			{
				if (numRemoved > 0) 
					Globals::logger.LogLongIndent("  types left:", (int)(this->copTypeToEntry.size()));
			}

			return numRemoved;
		}


		int GetCapacity(const vault copType) const
		{
			const auto foundType = this->copTypeToEntry.find(copType);
			return (foundType != this->copTypeToEntry.end()) ? foundType->second.capacity : 0;
		}


		bool UpdateCapacity
		(
			const vault copType,
			const int   change
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


		void LogContents(const char* const tableName) const
		{
			Globals::logger.LogLongIndent(tableName, this->maxTotalCopCapacity);

			for (const auto& pair : this->copTypeToEntry)
			{
				const std::string copName = std::format("  {:22}", this->copTypeToName.at(pair.first));
				Globals::logger.LogLongIndent(copName, pair.second.capacity, pair.second.chance);
			}
		}


		int GetMaxTotalCopCapacity() const {return this->maxTotalCopCapacity;}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<SpawnTable> eventSpawnTables;
	HeatParameters::Pair<SpawnTable> patrolSpawnTables;
	HeatParameters::Pair<SpawnTable> pursuitSpawnTables;
	HeatParameters::Pair<SpawnTable> roadblockSpawnTables;

	// Code caves
	int currentMaxCopCapacity = 0;





	// Auxiliary functinos --------------------------------------------------------------------------------------------------------------------------

	void ParseTables
	(
		HeatParameters::Parser&             parser,
		const std::string&                  tableName,
		const std::string&                  format,
		HeatParameters::Values<SpawnTable>& spawnTables,
		const bool                          hasCounts
	) {
		std::vector<std::string> copNames;
		std::vector<int>         copCounts;
		std::vector<int>         copChances;

		std::string section;
		size_t      numberOfEntries;

		for (size_t heatLevel = 1; heatLevel <= HeatParameters::maxHeatLevel; heatLevel++)
		{
			section = std::vformat(format + tableName, std::make_format_args(heatLevel));

			if (hasCounts)
			{
				numberOfEntries = parser.ParseParameterTable
				(
					section,
					copNames,
					ConfigParser::UserParameter<int>(copCounts,  1),
					ConfigParser::UserParameter<int>(copChances, 1)
				);
			}
			else numberOfEntries = parser.ParseUserParameter<int>(section, copNames, copChances, 1);

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
		// Heat parameters
		if (not parser.LoadFile(HeatParameters::configPathAdvanced + "Cars.ini")) return false;

		for (const bool forRaces : {false, true})
		{
			const std::string format = (forRaces) ? "Race{:02}:" : "Heat{:02}:";

			ParseTables(parser, "Events",     format, eventSpawnTables    .Get(forRaces), true);
			ParseTables(parser, "Patrols",    format, patrolSpawnTables   .Get(forRaces), false);
			ParseTables(parser, "Chasers",    format, pursuitSpawnTables  .Get(forRaces), true);
			ParseTables(parser, "Roadblocks", format, roadblockSpawnTables.Get(forRaces), true);
		}

		// Check for and deal with empty tables
		for (const SpawnTable& table : pursuitSpawnTables.roam) if (table.IsEmpty()) return false;

		ReplaceEmptyTables(eventSpawnTables.roam,     pursuitSpawnTables.roam);
		ReplaceEmptyTables(patrolSpawnTables.roam,    pursuitSpawnTables.roam);
		ReplaceEmptyTables(roadblockSpawnTables.roam, pursuitSpawnTables.roam);

		ReplaceEmptyTables(eventSpawnTables.race,     eventSpawnTables.roam);
		ReplaceEmptyTables(patrolSpawnTables.race,    patrolSpawnTables.roam);
		ReplaceEmptyTables(pursuitSpawnTables.race,   pursuitSpawnTables.roam);
		ReplaceEmptyTables(roadblockSpawnTables.race, roadblockSpawnTables.roam);

		return (featureEnabled = true);
	}



	void Validate()
	{
		if (not featureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [TAB] CopSpawnTables");

		for (const bool forRaces : {false, true})
		{
			for (size_t heatLevel = 1; heatLevel <= HeatParameters::maxHeatLevel; heatLevel++)
			{
				// With logging disabled, the compiler optimises all arguments away
				(eventSpawnTables    .Get(forRaces)[heatLevel - 1]).RemoveInvalidTypes("Events",     forRaces, heatLevel);
				(patrolSpawnTables   .Get(forRaces)[heatLevel - 1]).RemoveInvalidTypes("Patrols",    forRaces, heatLevel);
				(pursuitSpawnTables  .Get(forRaces)[heatLevel - 1]).RemoveInvalidTypes("Chasers",    forRaces, heatLevel);
				(roadblockSpawnTables.Get(forRaces)[heatLevel - 1]).RemoveInvalidTypes("Roadblocks", forRaces, heatLevel);
			}
		}
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		eventSpawnTables    .SetToHeat(isRacing, heatLevel);
		patrolSpawnTables   .SetToHeat(isRacing, heatLevel);
		pursuitSpawnTables  .SetToHeat(isRacing, heatLevel);
		roadblockSpawnTables.SetToHeat(isRacing, heatLevel);

		currentMaxCopCapacity = pursuitSpawnTables.current->GetMaxTotalCopCapacity();

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [TAB] CopSpawnTables");

			eventSpawnTables    .current->LogContents("Events                  ");
			patrolSpawnTables   .current->LogContents("Patrols                 ");
			pursuitSpawnTables  .current->LogContents("Chasers                 ");
			roadblockSpawnTables.current->LogContents("Roadblocks              ");
		}
	}
}