#pragma once

#include <array>
#include <string>
#include <format>
#include <vector>
#include <utility>
#include <string_view>

#include "Globals.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace CopSpawnTables
{

	// SpawnTable class -----------------------------------------------------------------------------------------------------------------------------

	class SpawnTable
	{
	private:

		// Internal struct
		struct Entry
		{
			const char* name;

			int count;    // max. cars
			int chance;   // relative
			int capacity; // available cars
		};



	private:

		int totalCopCount         = 0;
		int currentTotalCopChance = 0;

		ModContainers::VaultMap<Entry> copTypeToEntry;

		inline static constinit ModContainers::StableVaultMap<const std::string> copTypeToName;
		

		static auto RegisterName(const char* const copName)
		{
			const auto [pair, wasAdded] = SpawnTable::copTypeToName.try_emplace(Globals::GetVaultKey(copName), copName);

			return pair;
		}



	public:

		static const char* ConvertTypeToName(const vault copType)
		{
			const auto foundType = SpawnTable::copTypeToName.find(copType);
			return (foundType != SpawnTable::copTypeToName.end()) ? foundType->second->c_str() : nullptr;
		}


		bool ContainsType(const vault copType) const
		{
			return this->copTypeToEntry.contains(copType);
		}


		bool AddTypeByName
		(
			const char* const copName,
			const int         copCount, 
			const int         copChance
		) {
			if (not copName) return false;

			if (copCount  < 1) return false;
			if (copChance < 1) return false;
			
			const auto registration = this->RegisterName(copName);

			const auto [pair, wasAdded] = this->copTypeToEntry.try_emplace
			(
				registration->first,
				registration->second->c_str(), // safe due to pointer stability
				copCount, 
				copChance, 
				copCount
			);

			if (wasAdded)
			{
				this->totalCopCount         += copCount;
				this->currentTotalCopChance += copChance;
			}

			return wasAdded;
		}


		bool IsEmpty() const 
		{
			return (this->totalCopCount < 1);
		}


		bool HasCapacity() const
		{
			return (this->currentTotalCopChance > 0);
		}


		int GetCount(const vault copType) const
		{
			const auto foundType = this->copTypeToEntry.find(copType);
			return (foundType != this->copTypeToEntry.end()) ? foundType->second.count : 0;
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

			Entry&     copEntry     = foundType->second;
			const bool wasAvailable = (copEntry.capacity > 0);

			copEntry.capacity += change;

			if (copEntry.capacity > copEntry.count)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [TAB] Exceeded count for", copEntry.name);

				copEntry.capacity = copEntry.count;
			}

			const bool isAvailable = (copEntry.capacity > 0);

			if (wasAvailable != isAvailable)
			{
				if (wasAvailable)
					this->currentTotalCopChance -= copEntry.chance;
				
				else
					this->currentTotalCopChance += copEntry.chance;
			}

			return true;
		}


		const char* GetNameOfAvailableCop() const
		{
			if (this->HasCapacity())
			{
				int cumulativeChance = 0;

				const int chanceThreshold = Globals::prng.GenerateNumber<int>(1, this->currentTotalCopChance);

				for (const auto& [copType, copEntry] : this->copTypeToEntry)
				{
					if (copEntry.capacity > 0)
					{
						cumulativeChance += copEntry.chance;

						if (cumulativeChance >= chanceThreshold)
							return copEntry.name;
					}
				}

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [TAB] Failed to select vehicle:", cumulativeChance, chanceThreshold);
			}

			return nullptr;
		}


		bool Validate
		(
			const std::string_view tableName,
			const bool             forRaces,
			const size_t           heatLevel
		) {
			size_t numRemoved = 0;

			auto iterator = this->copTypeToEntry.begin();

			while (iterator != this->copTypeToEntry.end())
			{
				if (not Globals::IsVehicleTypeCar(iterator->first))
				{
					const Entry& copEntry = iterator->second;

					if constexpr (Globals::loggingEnabled)
					{
						if (numRemoved == 0)
							Globals::logger.Log<2>(tableName, static_cast<int>(heatLevel), (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>('-', copEntry.name, copEntry.capacity, copEntry.chance);
					}

					this->totalCopCount -= copEntry.count;

					if (copEntry.capacity > 0)
						this->currentTotalCopChance -= copEntry.chance;

					iterator = this->copTypeToEntry.erase(iterator);

					++numRemoved;
				}
				else ++iterator;
			}

			if constexpr (Globals::loggingEnabled)
			{
				if (numRemoved > 0)
					Globals::logger.Log<3>(static_cast<int>(this->copTypeToEntry.size()), "type(s) left");
			}

			this->copTypeToEntry.shrink_to_fit();

			return (numRemoved == 0);
		}


		void Log(const std::string_view tableName) const
		{
			Globals::logger.Log<2>(tableName, this->totalCopCount);

			for (const auto& [copType, copEntry] : this->copTypeToEntry)
				Globals::logger.Log<3>(std::format("{:22}", copEntry.name), copEntry.capacity, '/', copEntry.chance);
		}
	};





	// Scoped aliases -------------------------------------------------------------------------------------------------------------------------------

	using Tables    = HeatParameters::Values     <SpawnTable>;
	using TablePair = HeatParameters::PointerPair<SpawnTable>;





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	constinit TablePair patrolSpawnTables;
	constinit TablePair chaserSpawnTables;
	constinit TablePair scriptedSpawnTables;
	constinit TablePair roadblockSpawnTables;





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ParseTables
	(
		const HeatParameters::Parser& parser,
		const std::string_view        tableName,
		const std::string_view        formatPrefix,
		Tables&                       tables
	) {
		std::vector<const char*> copNames; // for game compatibility

		std::vector<int> copCounts;
		std::vector<int> copChances;

		const std::string sectionFormat = std::format("{}:{}", formatPrefix, tableName);

		for (const size_t heatLevel : HeatParameters::heatLevels)
		{
			const std::string section    = std::vformat(sectionFormat, std::make_format_args(heatLevel));
			const size_t      numEntries = parser.ParseUser<const char*, int, int>(section, copNames, {copCounts, {0}}, {copChances, {0}});

			for (size_t entryID = 0; entryID < numEntries; ++entryID)
				(tables[heatLevel - 1]).AddTypeByName(copNames[entryID], copCounts[entryID], copChances[entryID]);
		}
	}



	bool ValidateTablePairs()
	{
		bool noVehiclesRemoved = true;

		for (const bool forRaces : {false, true})
		{
			Tables& patrolTables    = patrolSpawnTables   .GetValues(forRaces);
			Tables& chaserTables    = chaserSpawnTables   .GetValues(forRaces);
			Tables& scriptedTables  = scriptedSpawnTables .GetValues(forRaces);
			Tables& roadblockTables = roadblockSpawnTables.GetValues(forRaces);

			for (const size_t heatLevel : HeatParameters::heatLevels)
			{
				noVehiclesRemoved &= patrolTables   [heatLevel - 1].Validate("Patrols",    forRaces, heatLevel);
				noVehiclesRemoved &= chaserTables   [heatLevel - 1].Validate("Chasers",    forRaces, heatLevel);
				noVehiclesRemoved &= scriptedTables [heatLevel - 1].Validate("Scripted",   forRaces, heatLevel);
				noVehiclesRemoved &= roadblockTables[heatLevel - 1].Validate("Roadblocks", forRaces, heatLevel);

				if ((not forRaces) and chaserTables[heatLevel - 1].IsEmpty())
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<2>("No Chasers for Heat level", static_cast<int>(heatLevel));

					return false; // empty free-roam "Chasers" table
				}
			}
		}

		if constexpr (Globals::loggingEnabled)
		{
			if (noVehiclesRemoved)
				Globals::logger.Log<2>("All vehicles valid");
		}

		return true;
	}



	void ReplaceEmptyTables
	(
		Tables&       tables,
		const Tables& replacements
	) {
		for (const size_t heatLevel : HeatParameters::heatLevels)
			if ((tables[heatLevel - 1]).IsEmpty()) tables[heatLevel - 1] = replacements[heatLevel - 1];
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [TAB] CopSpawnTables");

		if (not parser.LoadFile(HeatParameters::configPathAdvanced, "CarTables.ini")) return false;

		// Heat parameters
		for (const bool forRaces : {false, true})
		{
			const std::string_view format = (forRaces) ? "Race{:02}" : "Heat{:02}";

			ParseTables(parser, "Patrols",    format, patrolSpawnTables   .GetValues(forRaces));
			ParseTables(parser, "Chasers",    format, chaserSpawnTables   .GetValues(forRaces));
			ParseTables(parser, "Scripted",   format, scriptedSpawnTables .GetValues(forRaces));
			ParseTables(parser, "Roadblocks", format, roadblockSpawnTables.GetValues(forRaces));
		}

		if (not ValidateTablePairs()) return false; // empty free-roam "Chasers" table; disable feature

		ReplaceEmptyTables(patrolSpawnTables.roam,    chaserSpawnTables.roam);
		ReplaceEmptyTables(scriptedSpawnTables.roam,  chaserSpawnTables.roam);
		ReplaceEmptyTables(roadblockSpawnTables.roam, chaserSpawnTables.roam);

		ReplaceEmptyTables(patrolSpawnTables.race,    patrolSpawnTables.roam);
		ReplaceEmptyTables(chaserSpawnTables.race,    chaserSpawnTables.roam);
		ReplaceEmptyTables(scriptedSpawnTables.race,  scriptedSpawnTables.roam);
		ReplaceEmptyTables(roadblockSpawnTables.race, roadblockSpawnTables.roam);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [TAB] CopSpawnTables");

		patrolSpawnTables   .current->Log("Patrols                 ");
		chaserSpawnTables   .current->Log("Chasers                 ");
		scriptedSpawnTables .current->Log("Scripted                ");
		roadblockSpawnTables.current->Log("Roadblocks              ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		patrolSpawnTables   .SetToHeat(isRacing, heatLevel);
		chaserSpawnTables   .SetToHeat(isRacing, heatLevel);
		scriptedSpawnTables .SetToHeat(isRacing, heatLevel);
		roadblockSpawnTables.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}