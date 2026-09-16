#pragma once

#include <array>
#include <vector>
#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/StringTools.hpp"



namespace CopSpawnTables
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[TAB]";
	constexpr Globals::LogLiteral logName = "CopSpawnTables";





	// Entry class ----------------------------------------------------------------------------------------------------------------------------------

	class Entry
	{
	private: // members

		const char* copName; // C-style for game compatibility

		int numActive; // cars
		int maxCount;  // cars

		int chance; // relative


	public: // methods

		Entry
		(
			const char* const copName,
			const int         maxCount,
			const int         chance
		)
			: copName(copName), numActive(0), maxCount(maxCount), chance(chance)
		{
			Globals::vehicleNames.MakeIntern(this->copName);
		}


		[[nodiscard]] const char* GetCopName() const
		{
			return this->copName;
		}


		[[nodiscard]] int GetNumActive() const
		{
			return this->numActive;
		}


		int ChangeNumActive(const int change)
		{
			return (this->numActive += change);
		}


		void ResetNumActive()
		{
			this->numActive = 0;
		}


		[[nodiscard]] int GetMaxCount() const
		{
			return this->maxCount;
		}


		[[nodiscard]] int GetNumAvailable() const
		{
			return this->maxCount - this->numActive;
		}


		[[nodiscard]] bool IsAvailable() const
		{
			return (this->GetNumAvailable() > 0);
		}


		[[nodiscard]] int GetChance() const
		{
			return this->chance;
		}
	};





	// SpawnTable class -----------------------------------------------------------------------------------------------------------------------------

	class SpawnTable
	{
	private: // members

		int currentTotalCopChance = 0;

		ModContainers::VaultMap<Entry> copTypeToEntry;


	public: // methods

		bool CreateNewEntry
		(
			const char* const copName,
			const int         maxCount, 
			const int         chance
		) {
			if (maxCount < 1) return false;
			if (chance   < 1) return false;

			const vault copType = Globals::GetVaultHash(copName);
			if (not Globals::VehicleType::IsCar(copType)) return false;

			const auto [_, isNewType] = this->copTypeToEntry.try_emplace(copType, copName, maxCount, chance);

			if (isNewType)
				this->currentTotalCopChance += chance;

			return isNewType;
		}


		[[nodiscard]] size_t GetNumTypes() const
		{
			return this->copTypeToEntry.size();
		}


		[[nodiscard]] bool Contains(const vault copType) const
		{
			return this->copTypeToEntry.contains(copType);
		}


		[[nodiscard]] bool IsEmpty() const
		{
			return this->copTypeToEntry.empty();
		}


		[[nodiscard]] bool IsAnyCopAvailable() const
		{
			return (this->currentTotalCopChance > 0);
		}


		[[nodiscard]] int GetMaxCount(const vault copType) const
		{
			const Entry* const entry = this->copTypeToEntry.get(copType);
			return (entry) ? entry->GetMaxCount() : 0;
		}


		[[nodiscard]] int GetTotalMaxCount() const
		{
			int totalMaxCount = 0;

			for (const auto& [_, entry] : this->copTypeToEntry)
				totalMaxCount += entry.GetMaxCount();

			return totalMaxCount;
		}


		[[nodiscard]] int GetNumAvailable(const vault copType) const
		{
			const Entry* const entry = this->copTypeToEntry.get(copType);
			return (entry) ? entry->GetNumAvailable() : 0;
		}


		bool ChangeNumActive
		(
			const vault copType,
			const int   change
		) {
			Entry* const entry = this->copTypeToEntry.get(copType);
			if (not entry) return false; // unknown type

			const bool wasAvailable = entry->IsAvailable();

			if (entry->ChangeNumActive(change) < 0)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Miscounted", entry->GetCopName());

				ASSERT_UNREACHABLE;
			}

			if (wasAvailable != entry->IsAvailable())
			{
				this->currentTotalCopChance += (wasAvailable) ? -entry->GetChance() : +entry->GetChance();

				if (this->currentTotalCopChance < 0)
				{
					if constexpr (Globals::loggingEnabled)
						Globals::LogWarning(logTag, "Miscounted total cop chance");

					ASSERT_UNREACHABLE;
				}
			}

			return true;
		}


		bool ResetNumActive(const vault copType)
		{
			Entry* const entry = this->copTypeToEntry.get(copType);
			if (not entry) return false; // unknown type

			if (not entry->IsAvailable())
				this->currentTotalCopChance += entry->GetChance();

			entry->ResetNumActive();

			return true;
		}


		void ResetNumActive()
		{
			this->currentTotalCopChance = 0;

			for (auto& [_, entry] : this->copTypeToEntry)
			{
				entry.ResetNumActive();

				this->currentTotalCopChance += entry.GetChance();
			}
		}


		[[nodiscard]] const char* GetNameOfAvailableCop() const
		{
			if (not this->IsAnyCopAvailable()) return nullptr;

			int       cumulativeChance = 0;
			const int chanceThreshold  = Globals::pRNG.GenerateNumber<int>(1, this->currentTotalCopChance);

			for (const auto& [_, entry] : this->copTypeToEntry)
			{
				if (not entry.IsAvailable()) continue;

				cumulativeChance += entry.GetChance();

				if (cumulativeChance >= chanceThreshold)
					return entry.GetCopName();
			}

			if constexpr (Globals::loggingEnabled)
				Globals::LogWarning(logTag, "Failed to select vehicle");

			ASSERT_UNREACHABLE_THEN(return nullptr);
		}


		void Log(const Globals::LogLiteral header) const
		{
			static RELEASE_CONSTINIT StringTools::FormatBuffer buffer;

			HeatParameters::LogParameter(header, this->GetTotalMaxCount());

			for (const auto& [_, copEntry] : this->copTypeToEntry)
				Globals::LogDetail(buffer.Format("{:<22}", copEntry.GetCopName()), copEntry.GetMaxCount(), '/', copEntry.GetChance());
		}
	};





	// Feature setup (continued) --------------------------------------------------------------------------------------------------------------------

	// Heat parameters
	RELEASE_CONSTINIT HEAT_PARAMETER_OBJECT(SpawnTable, chasersTable);

	RELEASE_CONSTINIT HEAT_PARAMETER_OBJECT(SpawnTable, patrolsTable);

	RELEASE_CONSTINIT HEAT_PARAMETER_OBJECT(SpawnTable, scriptedTable);

	RELEASE_CONSTINIT HEAT_PARAMETER_OBJECT(SpawnTable, roadblockTable);


	


	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	bool ExtractTableObject
	(
		const ConfigParser::Parser&         parser,
		const std::string_view              tableName,
		HeatParameters::Object<SpawnTable>& tableObject
	) {
		bool allEntriesValid = true;

		StringTools::FormatBuffer buffer;

		std::vector<const char*> copNames;
		std::vector<int>         maxCounts;
		std::vector<int>         chances;

		for (const bool forRaces : {false, true})
		{
			auto& tableArray = tableObject.GetHeatLevelArray(forRaces);

			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				// Extract spawn-table entries
				const auto   sectionName = buffer.Format("{}{:02}:{}", (forRaces) ? "Race" : "Heat", heatLevelID + 1, tableName);
				const size_t numEntries  = parser.ExtractVectors<const char*, int, int>(sectionName, copNames, {maxCounts, {1}}, {chances, {1}});

				// Attempt to add new entries
				bool theseEntriesValid = true;

				SpawnTable& levelTable = tableArray[heatLevelID];

				for (size_t entryID = 0; entryID < numEntries; ++entryID)
				{
					if (levelTable.CreateNewEntry(copNames[entryID], maxCounts[entryID], chances[entryID])) continue;

					if constexpr (Globals::loggingEnabled)
					{
						if (theseEntriesValid)
							Globals::LogPlain(tableName, Globals::LogDec(heatLevelID + 1), (forRaces) ? "(race)" : "(roam)");

						Globals::LogDetail('-', copNames[entryID], maxCounts[entryID], chances[entryID]);
					}

					theseEntriesValid = false;
				}

				if constexpr (Globals::loggingEnabled)
				{
					if (not theseEntriesValid)
						Globals::LogDetail(Globals::LogDec(levelTable.GetNumTypes()), "type(s) left");
				}

				allEntriesValid &= theseEntriesValid;
			}
		}

		return allEntriesValid;
	}



	bool ExtractSpawnTablePointers(const ConfigParser::Parser& parser)
	{
		// All free-roam "Chasers" tables must be non-empty to serve as fallbacks
		bool allTableEntriesValid = ExtractTableObject(parser, "Chasers", chasersTable);

		for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
		{
			if (not chasersTable.roam[heatLevelID].IsEmpty()) continue;

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain("No Chasers for Heat level", Globals::LogDec(heatLevelID + 1));

			return false; // empty free-roam "Chasers" table
		}
		
		// Extract non-"Chasers" tables (may be empty)
		allTableEntriesValid &= ExtractTableObject(parser, "Patrols",    patrolsTable);
		allTableEntriesValid &= ExtractTableObject(parser, "Scripted",   scriptedTable);
		allTableEntriesValid &= ExtractTableObject(parser, "Roadblocks", roadblockTable);

		if constexpr (Globals::loggingEnabled)
		{
			if (allTableEntriesValid)
				Globals::LogPlain("All vehicles valid");
		}

		// Replace all (now-)empty spawn tables
		for (auto* const tableObject : {&chasersTable, &patrolsTable, &scriptedTable, &roadblockTable})
		{
			for (const size_t heatLevelID : HeatParameters::heatLevelIDs)
			{
				auto& roam = tableObject->roam[heatLevelID];
				auto& race = tableObject->race[heatLevelID];

				if (roam.IsEmpty()) roam = chasersTable.roam[heatLevelID];
				if (race.IsEmpty()) race = roam;
			}
		}

		return true;
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathAdvanced, Globals::fileCarTables)) return false;

		// Heat parameters
		if (not ExtractSpawnTablePointers(parser)) return false; // free-roam "Chasers" table(s) empty; disable feature

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		chasersTable.SetToHeatState(state);

		patrolsTable.SetToHeatState(state);

		scriptedTable.SetToHeatState(state);

		roadblockTable.SetToHeatState(state);
	}
}