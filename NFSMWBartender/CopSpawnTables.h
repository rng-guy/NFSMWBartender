#pragma once

#include <memory>
#include <string>
#include <format>
#include <vector>

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"



namespace CopSpawnTables
{

	// SpawnTable class -----------------------------------------------------------------------------------------------------------------------------

	class SpawnTable
	{
	private:

		struct Entry
		{
			const char* name;

			int count;
			int chance;
			int capacity;
		};

		int totalCopCount         = 0;
		int currentTotalCopChance = 0;

		HashContainers::VaultMap<Entry> copTypeToEntry;

		inline static HashContainers::StableVaultMap<std::string> copTypeToName;
		

		static auto RegisterName(const std::string& copName)
		{
			const vault copType          = Globals::GetVaultKey(copName.c_str());
			const auto  [pair, wasAdded] = SpawnTable::copTypeToName.try_emplace(copType, nullptr);

			if (wasAdded)
				pair->second = std::make_unique<std::string>(copName);

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
			const std::string& copName,
			const int          copCount, 
			const int          copChance
		) {
			if (copCount  < 1) return false;
			if (copChance < 1) return false;
			
			const auto registration = this->RegisterName(copName);

			const auto [pair, wasAdded] = this->copTypeToEntry.try_emplace
			(
				registration->first,
				registration->second->c_str(),
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


		const char* GetRandomAvailableCop() const
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
			const std::string& tableName,
			const bool         forRaces,
			const size_t       heatLevel
		) {
			size_t numRemoved = 0;
			auto   iterator   = this->copTypeToEntry.begin();

			while (iterator != this->copTypeToEntry.end())
			{
				if (not Globals::IsVehicleCar(iterator->first))
				{
					const Entry& copEntry = iterator->second;

					// With logging disabled, the compiler optimises all parameters away
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

			return (numRemoved == 0);
		}


		void Log(const std::string& tableName) const
		{
			Globals::logger.Log<2>(tableName, this->totalCopCount);

			for (const auto& [copType, copEntry] : this->copTypeToEntry)
				Globals::logger.Log<3>(std::format("{:22}", copEntry.name), copEntry.capacity, copEntry.chance);
		}
	};





	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	HeatParameters::PointerPair<SpawnTable> patrolSpawnTables;
	HeatParameters::PointerPair<SpawnTable> chaserSpawnTables;
	HeatParameters::PointerPair<SpawnTable> scriptedSpawnTables;
	HeatParameters::PointerPair<SpawnTable> roadblockSpawnTables;





	// Auxiliary functinos --------------------------------------------------------------------------------------------------------------------------

	void ParseTables
	(
		HeatParameters::Parser&             parser,
		const std::string&                  tableName,
		const std::string&                  format,
		HeatParameters::Values<SpawnTable>& spawnTables
	) {
		std::vector<std::string> copNames;

		std::vector<int> copCounts;
		std::vector<int> copChances;

		std::string section;

		for (const size_t heatLevel : HeatParameters::heatLevels)
		{
			section = std::vformat(format + tableName, std::make_format_args(heatLevel));

			const size_t numEntries = parser.ParseUser<int, int>(section, copNames, {copCounts, {1}}, {copChances, {1}});

			for (size_t entryID = 0; entryID < numEntries; ++entryID)
				spawnTables[heatLevel - 1].AddTypeByName(copNames[entryID], copCounts[entryID], copChances[entryID]);
		}
	}



	void ReplaceEmptyTables
	(
		HeatParameters::Values<SpawnTable>&       tables,
		const HeatParameters::Values<SpawnTable>& replacements
	) {
		for (const size_t heatLevel : HeatParameters::heatLevels)
			if (tables[heatLevel - 1].IsEmpty()) tables[heatLevel - 1] = replacements[heatLevel - 1];
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
			const std::string format = (forRaces) ? "Race{:02}:" : "Heat{:02}:";

			ParseTables(parser, "Patrols",    format, patrolSpawnTables   .GetValues(forRaces));
			ParseTables(parser, "Chasers",    format, chaserSpawnTables   .GetValues(forRaces));
			ParseTables(parser, "Scripted",   format, scriptedSpawnTables .GetValues(forRaces));
			ParseTables(parser, "Roadblocks", format, roadblockSpawnTables.GetValues(forRaces));
		}

		// Validation
		bool allValid = true;

		for (const bool forRaces : {false, true})
		{
			auto& patrolTables    = patrolSpawnTables   .GetValues(forRaces);
			auto& chaserTables    = chaserSpawnTables   .GetValues(forRaces);
			auto& scriptedTables  = scriptedSpawnTables .GetValues(forRaces);
			auto& roadblockTables = roadblockSpawnTables.GetValues(forRaces);

			for (const size_t heatLevel : HeatParameters::heatLevels)
			{
				// With logging disabled, the compiler optimises the boolean and all arguments away
				allValid &= (patrolTables   [heatLevel - 1]).Validate("Patrols",    forRaces, heatLevel);
				allValid &= (chaserTables   [heatLevel - 1]).Validate("Chasers",    forRaces, heatLevel);
				allValid &= (scriptedTables [heatLevel - 1]).Validate("Scripted",   forRaces, heatLevel);
				allValid &= (roadblockTables[heatLevel - 1]).Validate("Roadblocks", forRaces, heatLevel);

				if ((not forRaces) and chaserTables[heatLevel - 1].IsEmpty())
				{
					if constexpr (Globals::loggingEnabled)
						Globals::logger.Log<2>("No Chasers for Heat level", static_cast<int>(heatLevel));

					return false;
				}
			}
		}

		if constexpr (Globals::loggingEnabled)
		{
			if (allValid)
				Globals::logger.Log<2>("All vehicles valid");
		}

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