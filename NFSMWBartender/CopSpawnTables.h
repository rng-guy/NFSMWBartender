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
		struct CopEntry
		{
			const char* copName; // C-style for game compatibility

			int numActive;
			int maxCount;

			int chance; // relative


			[[nodiscard]] int GetNumAvailable() const
			{
				return this->maxCount - this->numActive;
			}


			[[nodiscard]] bool IsAvailable() const
			{
				return (this->GetNumAvailable() > 0);
			}
		};


	private:

		int currentTotalCopChance = 0;

		ModContainers::VaultMap<CopEntry> copTypeToEntry;


	public:

		[[nodiscard]] bool ContainsCopType(const vault copType) const
		{
			return this->copTypeToEntry.contains(copType);
		}


		bool AddCopEntry
		(
			const char* copName,
			const int   copCount, 
			const int   copChance
		) {
			const vault copType = Globals::GetVaultHash(copName);
			if (not Globals::IsVehicleTypeCar(copType)) return false;

			HeatParameters::MakePersistentString(copType, copName);

			const auto [pairIt, isNewType] = this->copTypeToEntry.try_emplace
			(
				copType, 
				copName,
				/* numActive = */ 0,  
				copCount,
				copChance
			);

			if (isNewType and (copCount > 0))
				this->currentTotalCopChance += copChance;

			return isNewType;
		}


		[[nodiscard]] size_t GetNumCopEntries() const
		{
			return this->copTypeToEntry.size();
		}


		[[nodiscard]] bool IsEmpty() const
		{
			return this->copTypeToEntry.empty();
		}


		[[nodiscard]] bool IsAnyCopAvailable() const
		{
			return (this->currentTotalCopChance > 0);
		}


		[[nodiscard]] int GetMaxCopCount(const vault copType) const
		{
			const auto foundType = this->copTypeToEntry.find(copType);
			if (foundType == this->copTypeToEntry.end()) return 0;

			return foundType->second.maxCount;
		}


		[[nodiscard]] int GetTotalMaxCopCount() const
		{
			int totalCopCount = 0;

			for (const auto& [copType, copEntry] : this->copTypeToEntry)
				totalCopCount += copEntry.maxCount;

			return totalCopCount;
		}


		[[nodiscard]] int GetNumAvailableCops(const vault copType) const
		{
			const auto foundType = this->copTypeToEntry.find(copType);
			if (foundType == this->copTypeToEntry.end()) return 0;

			return foundType->second.GetNumAvailable();
		}


		bool ChangeNumActiveCops
		(
			const vault copType,
			const int   change
		) {
			const auto foundType = this->copTypeToEntry.find(copType);
			if (foundType == this->copTypeToEntry.end()) return false;

			CopEntry&  copEntry     = foundType->second;
			const bool wasAvailable = copEntry.IsAvailable();

			copEntry.numActive += change;

			if constexpr (Globals::loggingEnabled)
			{
				if (copEntry.numActive < 0)
					Globals::logger.Log("WARNING: [TAB] Negative active count for", copEntry.copName);
			}

			if (wasAvailable != copEntry.IsAvailable())
			{
				if (wasAvailable)
					this->currentTotalCopChance -= copEntry.chance;
				
				else
					this->currentTotalCopChance += copEntry.chance;
			}

			return true;
		}


		void ResetActiveCopCounts()
		{
			this->currentTotalCopChance = 0;

			for (auto& [copType, copEntry] : this->copTypeToEntry)
			{
				copEntry.numActive           = 0;
				this->currentTotalCopChance += copEntry.chance;
			}
		}


		[[nodiscard]] const char* GetNameOfAvailableCop() const
		{
			if (not this->IsAnyCopAvailable()) return nullptr;

			int       cumulativeChance = 0;
			const int chanceThreshold  = Globals::prng.GenerateNumber<int>(1, this->currentTotalCopChance);

			for (const auto& [copType, copEntry] : this->copTypeToEntry)
			{
				if (not copEntry.IsAvailable()) continue;

				cumulativeChance += copEntry.chance;

				if (cumulativeChance >= chanceThreshold)
					return copEntry.copName;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [TAB] Failed to select vehicle:", cumulativeChance, chanceThreshold);

			return nullptr; // should never happen
		}


		void Log(const std::string_view tableName) const
		{
			Globals::logger.Log<2>(tableName, this->GetTotalMaxCopCount());

			for (const auto& [copType, copEntry] : this->copTypeToEntry)
				Globals::logger.Log<3>(copEntry.copName, copEntry.maxCount, '/', copEntry.chance);
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Heat parameters
	RELEASE_CONSTINIT HeatParameters::Pointer<SpawnTable> chaserSpawnTable;

	RELEASE_CONSTINIT HeatParameters::Pointer<SpawnTable> patrolSpawnTable;

	RELEASE_CONSTINIT HeatParameters::Pointer<SpawnTable> scriptedSpawnTable;

	RELEASE_CONSTINIT HeatParameters::Pointer<SpawnTable> roadblockSpawnTable;


	


	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseTablePointer
	(
		const HeatParameters::Parser&        parser,
		const std::string_view               tableName,
		HeatParameters::Pointer<SpawnTable>& tablePointer
	) {
		bool allEntriesValid = true;

		std::vector<const char*> copNames; // C-style for game compatibility
		std::vector<int>         copCounts;
		std::vector<int>         copChances;

		for (const bool forRaces : {false, true})
		{
			auto& tableArray = tablePointer.GetHeatLevelArray(forRaces);

			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				// Parse spawn-table entries
				const size_t      heatLevel  = heatLevelID + 1;
				const std::string section    = std::format("{}{:02}:{}", (forRaces) ? "Race" : "Heat", heatLevel, tableName);
				const size_t      numEntries = parser.ParseUser<const char*, int, int>(section, copNames, {copCounts, {1}}, {copChances, {1}});

				// Attempt to add new entries to table
				bool  theseEntriesValid = true;
				auto& levelTable        = tableArray[heatLevelID];

				for (size_t entryID = 0; entryID < numEntries; ++entryID)
				{
					if (levelTable.AddCopEntry(copNames[entryID], copCounts[entryID], copChances[entryID])) continue;

					if constexpr (Globals::loggingEnabled)
					{
						if (theseEntriesValid)
							Globals::logger.Log<2>(tableName, DecFormat(heatLevel), (forRaces) ? "(race)" : "(roam)");

						Globals::logger.Log<3>('-', copNames[entryID], copCounts[entryID], copChances[entryID]);
					}

					theseEntriesValid = false;
				}

				if constexpr (Globals::loggingEnabled)
				{
					if (not theseEntriesValid)
						Globals::logger.Log<3>(DecFormat(levelTable.GetNumCopEntries()), "type(s) left");
				}

				allEntriesValid &= theseEntriesValid;
			}
		}

		return allEntriesValid;
	}



	bool ParseSpawnTables(const HeatParameters::Parser& parser)
	{
		// All free-roam "Chasers" tables must be non-empty to serve as fallbacks
		bool allTableEntriesValid = ParseTablePointer(parser, "Chasers", chaserSpawnTable);

		for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
		{
			if (not chaserSpawnTable.roam[heatLevelID].IsEmpty()) continue;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>("No Chasers for Heat level", DecFormat(heatLevelID + 1));

			return false; // empty free-roam "Chasers" table
		}
		
		// Parse non-"Chasers" tables (may be empty)
		allTableEntriesValid &= ParseTablePointer(parser, "Patrols",    patrolSpawnTable);
		allTableEntriesValid &= ParseTablePointer(parser, "Scripted",   scriptedSpawnTable);
		allTableEntriesValid &= ParseTablePointer(parser, "Roadblocks", roadblockSpawnTable);

		if constexpr (Globals::loggingEnabled)
		{
			if (allTableEntriesValid)
				Globals::logger.Log<2>("All vehicles valid");
		}

		// Replace all (now-)empty spawn tables
		for (auto* const tablePointer : {&chaserSpawnTable, &patrolSpawnTable, &scriptedSpawnTable, &roadblockSpawnTable})
		{
			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				auto& roam = tablePointer->roam[heatLevelID];
				auto& race = tablePointer->race[heatLevelID];

				// all free-roam "Chasers" tables are guaranteed to be non-empty at this point
				if (roam.IsEmpty()) roam = chaserSpawnTable.roam[heatLevelID];
				if (race.IsEmpty()) race = roam;
			}
		}

		return true;
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [TAB] CopSpawnTables");

		if (not parser.LoadFile(HeatParameters::configPathAdvanced, "CarTables.ini")) return false;

		// Heat parameters
		if (not ParseSpawnTables(parser)) return false; // free-roam "Chasers" table(s) empty; disable feature

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void LogHeatStateReport()
	{
		Globals::logger.Log("    HEAT [TAB] CopSpawnTables");

		chaserSpawnTable.current->Log("Chasers                 ");

		patrolSpawnTable.current->Log("Patrols                 ");

		scriptedSpawnTable.current->Log("Scripted                ");

		roadblockSpawnTable.current->Log("Roadblocks              ");
	}



	void SetToHeatState
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not anyFeatureEnabled) return;

		chaserSpawnTable.SetToHeatState(isRacing, heatLevel);

		patrolSpawnTable.SetToHeatState(isRacing, heatLevel);

		scriptedSpawnTable.SetToHeatState(isRacing, heatLevel);

		roadblockSpawnTable.SetToHeatState(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatStateReport();
	}
}