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
			return pair; // for immediate iterator access to (potentially) inserted pair
		}



	public:

		[[nodiscard]] static const char* ConvertTypeToName(const vault copType)
		{
			const auto foundType = SpawnTable::copTypeToName.find(copType);
			return (foundType != SpawnTable::copTypeToName.end()) ? foundType->second->c_str() : nullptr;
		}


		[[nodiscard]] bool ContainsType(const vault copType) const
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
			
			const auto registration     = this->RegisterName(copName);
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


		[[nodiscard]] bool IsEmpty() const
		{
			return (this->totalCopCount < 1);
		}


		[[nodiscard]] bool HasCapacity() const
		{
			return (this->currentTotalCopChance > 0);
		}


		[[nodiscard]] int GetCount(const vault copType) const
		{
			const auto foundType = this->copTypeToEntry.find(copType);
			return (foundType != this->copTypeToEntry.end()) ? foundType->second.count : 0;
		}


		[[nodiscard]] int GetCapacity(const vault copType) const
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


		[[nodiscard]] const char* GetNameOfAvailableCop() const
		{
			if (this->HasCapacity())
			{
				int       cumulativeChance = 0;
				const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, this->currentTotalCopChance);

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
			bool allValid = true;
			auto iterator = this->copTypeToEntry.begin();

			while (iterator != this->copTypeToEntry.end())
			{
				if (not Globals::IsVehicleTypeCar(iterator->first))
				{
					const Entry& copEntry = iterator->second;

					if constexpr (Globals::loggingEnabled)
					{
						if (allValid)
							Globals::logger.Log<2>(tableName, static_cast<int>(heatLevel), (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>('-', copEntry.name, copEntry.capacity, copEntry.chance);
					}

					this->totalCopCount -= copEntry.count;

					if (copEntry.capacity > 0)
						this->currentTotalCopChance -= copEntry.chance;

					iterator = this->copTypeToEntry.erase(iterator);
					allValid = false;
				}
				else ++iterator;
			}

			if constexpr (Globals::loggingEnabled)
			{
				if (not allValid)
					Globals::logger.Log<3>(static_cast<int>(this->copTypeToEntry.size()), "type(s) left");
			}

			this->copTypeToEntry.shrink_to_fit();

			return allValid;
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
	constinit TablePair chaserSpawnTables;
	constinit TablePair patrolSpawnTables;
	constinit TablePair scriptedSpawnTables;
	constinit TablePair roadblockSpawnTables;





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseTablePair
	(
		const HeatParameters::Parser& parser,
		const std::string_view        tableName,
		TablePair&                    tablePair
	) {
		bool allValid = true;

		std::vector<const char*> copNames; // C-style for game compatibility
		std::vector<int>         copCounts;
		std::vector<int>         copChances;

		for (const bool forRaces : {false, true})
		{
			Tables&                tables = tablePair.GetValues(forRaces);
			const std::string_view format = (forRaces) ? "Race{:02}:{}" : "Heat{:02}:{}";

			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				const size_t      heatLevel  = heatLevelID + 1; // make_format_args doesn't accept rvalues
				const std::string section    = std::vformat(format, std::make_format_args(heatLevel, tableName));
				const size_t      numEntries = parser.ParseUser<const char*, int, int>(section, copNames, {copCounts, {0}}, {copChances, {0}});

				for (size_t entryID = 0; entryID < numEntries; ++entryID)
					tables[heatLevelID].AddTypeByName(copNames[entryID], copCounts[entryID], copChances[entryID]);

				allValid &= tables[heatLevelID].Validate(tableName, forRaces, heatLevel); // removes invalid vehicles
			}
		}

		return allValid;
	}



	bool ParseSpawnTables(const HeatParameters::Parser& parser)
	{
		// All free-roam "Chasers" tables must be non-empty to serve as fallbacks
		bool allValid = ParseTablePair(parser, "Chasers", chaserSpawnTables);

		for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
		{
			if (chaserSpawnTables.roam[heatLevelID].IsEmpty())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log<2>("No Chasers for Heat level", static_cast<int>(heatLevelID + 1));

				return false; // empty free-roam "Chasers" table
			}
		}
		
		// Parse non-"Chasers" tables (may be empty)
		allValid &= ParseTablePair(parser, "Patrols",    patrolSpawnTables);
		allValid &= ParseTablePair(parser, "Scripted",   scriptedSpawnTables);
		allValid &= ParseTablePair(parser, "Roadblocks", roadblockSpawnTables);

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.Log<2>("All vehicles valid");
		}

		// Replace all (now-)empty spawn tables
		for (TablePair* const tablePair : {&chaserSpawnTables, &patrolSpawnTables, &scriptedSpawnTables, &roadblockSpawnTables})
		{
			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				// all free-roam "Chasers" tables are guaranteed to be non-empty at this point
				if (tablePair->roam[heatLevelID].IsEmpty()) tablePair->roam[heatLevelID] = chaserSpawnTables.roam[heatLevelID];
				if (tablePair->race[heatLevelID].IsEmpty()) tablePair->race[heatLevelID] = tablePair->roam       [heatLevelID];
			}
		}

		return true;
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [TAB] CopSpawnTables");

		if (not parser.LoadFile(HeatParameters::configPathAdvanced, "CarTables.ini")) return false;

		// Heat parameters
		if (not ParseSpawnTables(parser)) return false; // free-roam "Chasers" table(s) empty; disable feature

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [TAB] CopSpawnTables");

		chaserSpawnTables   .current->Log("Chasers                 ");
		patrolSpawnTables   .current->Log("Patrols                 ");
		scriptedSpawnTables .current->Log("Scripted                ");
		roadblockSpawnTables.current->Log("Roadblocks              ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		chaserSpawnTables   .SetToHeat(isRacing, heatLevel);
		patrolSpawnTables   .SetToHeat(isRacing, heatLevel);
		scriptedSpawnTables .SetToHeat(isRacing, heatLevel);
		roadblockSpawnTables.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}