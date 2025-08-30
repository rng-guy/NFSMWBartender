#pragma once

#include "Globals.h"
#include "PursuitFeatures.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace StrategyOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<bool>  heavy3UnblockEnableds (false);
	HeatParameters::Pair<float> minHeavy3UnblockDelays(1.f);
	HeatParameters::Pair<float> maxHeavy3UnblockDelays(1.f);

	HeatParameters::Pair<bool>  heavy4UnblockEnableds (false);
	HeatParameters::Pair<float> minHeavy4UnblockDelays(1.f);
	HeatParameters::Pair<float> maxHeavy4UnblockDelays(1.f);

	HeatParameters::Pair<bool>  leader5UnblockEnableds (false);
	HeatParameters::Pair<float> minLeader5UnblockDelays(1.f);
	HeatParameters::Pair<float> maxLeader5UnblockDelays(1.f);

	HeatParameters::Pair<bool>  leader7UnblockEnableds (false);
	HeatParameters::Pair<float> minLeader7UnblockDelays(1.f);
	HeatParameters::Pair<float> maxLeader7UnblockDelays(1.f);





	// StrategyManager class ------------------------------------------------------------------------------------------------------------------------

	class StrategyManager : public PursuitFeatures::PursuitReaction
	{
		using CopLabel = PursuitFeatures::CopLabel;



	private:

		const address* const heavyStrategy  = (address*)(this->pursuit + 0x194);
		const address* const leaderStrategy = (address*)(this->pursuit + 0x198);

		bool watchingHeavyStrategy  = false;
		bool watchingLeaderStrategy = false;

		bool  unblockingEnabled = false;
		float spawnTimestamp    = *(this->simulationTime);
		float unblockTimestamp  = this->spawnTimestamp;

		inline static Globals::AddressMap<StrategyManager*> pursuitToManager;


		void Reset()
		{
			this->watchingHeavyStrategy  = false;
			this->watchingLeaderStrategy = false;
		}


		void GenerateUnblockTimestamp
		(
			const bool  enabled,
			const float minDelay,
			const float maxDelay
		) {
			this->unblockingEnabled = enabled;

			if (enabled)
				this->unblockTimestamp = this->spawnTimestamp + Globals::prng.Generate<float>(minDelay, maxDelay);
		}


		void UpdateHeavyTimestamp()
		{
			if (not this->IsHeatLevelKnown())    return;
			if (not this->watchingHeavyStrategy) return;

			const address strategy = *(this->heavyStrategy);

			if (not strategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Invalid HeavyStrategy pointer in", this->pursuit);

				this->Reset();

				return;
			}

			const int strategyType = *((int*)strategy);

			switch (strategyType)
			{
			case 3:
				this->GenerateUnblockTimestamp
				(
					heavy3UnblockEnableds.current,
					minHeavy3UnblockDelays.current,
					maxHeavy3UnblockDelays.current
				);
				break;

			case 4:
				this->GenerateUnblockTimestamp
				(
					heavy4UnblockEnableds.current,
					minHeavy4UnblockDelays.current,
					maxHeavy4UnblockDelays.current
				);
				break;

			default:
				this->Reset();
			}

			if constexpr (Globals::loggingEnabled)
			{
				if (this->watchingHeavyStrategy)
				{
					if (this->unblockingEnabled)
					{
						Globals::logger.Log
						(
							this->pursuit,
							"[STR] HeavyStrategy",
							strategyType,
							"blocking for",
							this->unblockTimestamp - *(this->simulationTime)
						);
					}
					else Globals::logger.Log(this->pursuit, "[STR] HeavyStrategy", strategyType, "blocking");
				}
				else Globals::logger.Log("WARNING: [STR] Invalid HeavyStrategy", strategyType, "in", this->pursuit);
			}
		}


		void WatchHeavyStrategy()
		{
			this->watchingHeavyStrategy  = *(this->heavyStrategy);
			this->watchingLeaderStrategy = false;

			if constexpr (Globals::loggingEnabled)
			{
				if (this->watchingHeavyStrategy)
					Globals::logger.Log(this->pursuit, "[STR] Watching HeavyStrategy", *((int*)(*(this->heavyStrategy))));

				else
					Globals::logger.Log("WARNING: [STR] Invalid HeavyStrategy pointer in", this->pursuit);
			}

			this->spawnTimestamp = *(this->simulationTime);
			this->UpdateHeavyTimestamp();
		}


		void UpdateLeaderTimestamp()
		{
			if (not this->IsHeatLevelKnown())     return;
			if (not this->watchingLeaderStrategy) return;

			const address strategy = *(this->leaderStrategy);

			if (not strategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Invalid LeaderStrategy pointer in", this->pursuit);

				this->Reset();

				return;
			}

			const int strategyType = *((int*)strategy);

			switch (strategyType)
			{
			case 5:
				this->GenerateUnblockTimestamp
				(
					leader5UnblockEnableds.current,
					minLeader5UnblockDelays.current,
					maxLeader5UnblockDelays.current
				);
				break;

			case 7:
				this->GenerateUnblockTimestamp
				(
					leader7UnblockEnableds.current,
					minLeader7UnblockDelays.current,
					maxLeader7UnblockDelays.current
				);
				break;

			default:
				this->Reset();
			}

			if constexpr (Globals::loggingEnabled)
			{
				if (this->watchingLeaderStrategy)
				{
					if (this->unblockingEnabled)
					{
						Globals::logger.Log
						(
							this->pursuit, 
							"[STR] LeaderStrategy", 
							strategyType,
							"blocking for", 
							this->unblockTimestamp - *(this->simulationTime)
						);
					}
					else Globals::logger.Log(this->pursuit, "[STR] LeaderStrategy", strategyType, "blocking");
				}
				else Globals::logger.Log("WARNING: [STR] Invalid LeaderStrategy", strategyType, "in", this->pursuit);
			}
		}


		void WatchLeaderStrategy()
		{
			this->watchingHeavyStrategy  = false;
			this->watchingLeaderStrategy = *(this->leaderStrategy);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->watchingLeaderStrategy)
					Globals::logger.Log(this->pursuit, "[STR] Watching LeaderStrategy", *((int*)(*(this->leaderStrategy))));

				else
					Globals::logger.Log("WARNING: [STR] Invalid LeaderStrategy pointer in", this->pursuit);
			}

			this->spawnTimestamp = *(this->simulationTime);
			this->UpdateLeaderTimestamp();
		}


		void CheckUnblockTimestamp() const
		{
			if (not this->IsHeatLevelKnown())                                      return;
			if (not this->unblockingEnabled)                                       return;
			if (not (this->watchingHeavyStrategy or this->watchingLeaderStrategy)) return;
			if (not (*(this->simulationTime) >= this->unblockTimestamp))           return;

			static void (__thiscall* const ClearSupportRequest)(address) = (void (__thiscall*)(address))0x42BCF0;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[STR] Unblocking requests");

			ClearSupportRequest(this->pursuit);
		}



	public:

		explicit StrategyManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			this->pursuitToManager.insert({this->pursuit, this});
		}


		~StrategyManager()
		{
			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnGameplay() override
		{
			this->CheckUnblockTimestamp();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateHeavyTimestamp();
			this->UpdateLeaderTimestamp();
			this->CheckUnblockTimestamp();
		}


		static void __fastcall NotifyOfHeavyStrategy(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
				foundManager->second->WatchHeavyStrategy();

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [STR] HeavyStrategy of unknown pursuit", pursuit);
		}


		static void __fastcall NotifyOfLeaderStrategy(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
				foundManager->second->WatchLeaderStrategy();

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [STR] LeaderStrategy of unknown pursuit", pursuit);
		}


		static void __fastcall NotifyOfClearedRequest(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
			{
				foundManager->second->Reset();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[STR] Request cleared");
			}
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address clearRequestEntrance = 0x42B431;
	constexpr address clearRequestExit     = 0x42B436;

	__declspec(naked) void ClearRequest()
	{
		__asm
		{
			mov dword ptr [ebp + 0x78], edi

			push ecx
			lea ecx, dword ptr [ebp - 0x194]
			mov dword ptr [ecx + 0x18C], 0x0             // number of active supports
			call StrategyManager::NotifyOfClearedRequest // ecx: pursuit
			pop ecx

			test cl, cl

			jmp dword ptr clearRequestExit
		}
	}



	constexpr address leaderStrategyEntrance = 0x41F824;
	constexpr address leaderStrategyExit     = 0x41F82F;

	__declspec(naked) void LeaderStrategy()
	{
		__asm
		{
			mov dword ptr [eax + 0x78], 0x2

			push eax
			lea ecx, dword ptr [eax - 0x194]
			call StrategyManager::NotifyOfLeaderStrategy
			pop eax

			mov ecx, dword ptr [eax + 0x4]
			pop edi

			jmp dword ptr leaderStrategyExit
		}
	}



	constexpr address heavyStrategy3Entrance = 0x41F455;
	constexpr address heavyStrategy3Exit     = 0x41F45F;

	__declspec(naked) void HeavyStrategy3()
	{
		__asm
		{
			mov dword ptr [eax + 0x78], 0x2

			push eax
			lea ecx, dword ptr [eax - 0x194]
			call StrategyManager::NotifyOfHeavyStrategy
			pop eax

			mov ecx, dword ptr [eax]
			pop edi

			jmp dword ptr heavyStrategy3Exit
		}
	}



	constexpr address heavyStrategy4Entrance = 0x43E7DF;
	constexpr address heavyStrategy4Exit     = 0x43E7E8;

	__declspec(naked) void HeavyStrategy4()
	{
		__asm
		{
			mov dword ptr [esi + 0x78], 0x2

			lea ecx, dword ptr [esi - 0x194]
			call StrategyManager::NotifyOfHeavyStrategy

			mov ecx, dword ptr [esi]

			jmp dword ptr heavyStrategy4Exit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if (not parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini")) return false;

		// Heat parameters
		featureEnabled |= HeatParameters::ParseOptionalInterval
		(
			parser,
			"Heavy3:Unblock",
			heavy3UnblockEnableds,
			minHeavy3UnblockDelays,
			maxHeavy3UnblockDelays
		);

		featureEnabled |= HeatParameters::ParseOptionalInterval
		(
			parser,
			"Heavy4:Unblock",
			heavy4UnblockEnableds,
			minHeavy4UnblockDelays,
			maxHeavy4UnblockDelays
		);

		featureEnabled |= HeatParameters::ParseOptionalInterval
		(
			parser,
			"Leader5:Unblock",
			leader5UnblockEnableds,
			minLeader5UnblockDelays,
			maxLeader5UnblockDelays
		);

		featureEnabled |= HeatParameters::ParseOptionalInterval
		(
			parser,
			"Leader7:Unblock",
			leader7UnblockEnableds,
			minLeader7UnblockDelays,
			maxLeader7UnblockDelays
		);

		if (not featureEnabled) return false;

		// Code caves
		MemoryEditor::DigCodeCave(ClearRequest,   clearRequestEntrance,   clearRequestExit);
		MemoryEditor::DigCodeCave(LeaderStrategy, leaderStrategyEntrance, leaderStrategyExit);
		MemoryEditor::DigCodeCave(HeavyStrategy3, heavyStrategy3Entrance, heavyStrategy3Exit);
		MemoryEditor::DigCodeCave(HeavyStrategy4, heavyStrategy4Entrance, heavyStrategy4Exit);

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		heavy3UnblockEnableds .SetToHeat(isRacing, heatLevel);
		minHeavy3UnblockDelays.SetToHeat(isRacing, heatLevel);
		maxHeavy3UnblockDelays.SetToHeat(isRacing, heatLevel);
		
		heavy4UnblockEnableds .SetToHeat(isRacing, heatLevel);
		minHeavy4UnblockDelays.SetToHeat(isRacing, heatLevel);
		maxHeavy4UnblockDelays.SetToHeat(isRacing, heatLevel);

		leader5UnblockEnableds .SetToHeat(isRacing, heatLevel);
		minLeader5UnblockDelays.SetToHeat(isRacing, heatLevel);
		maxLeader5UnblockDelays.SetToHeat(isRacing, heatLevel);

		leader7UnblockEnableds .SetToHeat(isRacing, heatLevel);
		minLeader7UnblockDelays.SetToHeat(isRacing, heatLevel);
		maxLeader7UnblockDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
		{
			Globals::logger.Log("    HEAT [STR] StrategyOverrides");

			Globals::logger.LogLongIndent("HeavyStrategy 3", (heavy3UnblockEnableds.current) ? "unblocked" : "blocks");

			if (heavy3UnblockEnableds.current)
			{
				Globals::logger.LogLongIndent("minHeavy3UnblockDelay   ", minHeavy3UnblockDelays.current);
				Globals::logger.LogLongIndent("maxHeavy3UnblockDelay   ", maxHeavy3UnblockDelays.current);
			}

			Globals::logger.LogLongIndent("HeavyStrategy 4", (heavy4UnblockEnableds.current) ? "unblocked" : "blocks");

			if (heavy4UnblockEnableds.current)
			{
				Globals::logger.LogLongIndent("minHeavy4UnblockDelay   ", minHeavy4UnblockDelays.current);
				Globals::logger.LogLongIndent("maxHeavy4UnblockDelay   ", maxHeavy4UnblockDelays.current);
			}

			Globals::logger.LogLongIndent("LeaderStrategy 5", (leader5UnblockEnableds.current) ? "unblocked" : "blocks");

			if (leader5UnblockEnableds.current)
			{
				Globals::logger.LogLongIndent("minLeader5UnblockDelay  ", minLeader5UnblockDelays.current);
				Globals::logger.LogLongIndent("maxLeader5UnblockDelay  ", maxLeader5UnblockDelays.current);
			}

			Globals::logger.LogLongIndent("LeaderStrategy 7", (leader7UnblockEnableds.current) ? "unblocked" : "blocks");

			if (leader7UnblockEnableds.current)
			{
				Globals::logger.LogLongIndent("minLeader7UnblockDelay  ", minLeader7UnblockDelays.current);
				Globals::logger.LogLongIndent("maxLeader7UnblockDelay  ", maxLeader7UnblockDelays.current);
			}
		}
	}
}