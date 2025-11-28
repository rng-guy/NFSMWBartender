#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace StrategyOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<float> racerSpeedThresholds(25.f); // kph
	
	HeatParameters::OptionalInterval heavy3UnblockDelays;
	HeatParameters::OptionalInterval heavy4UnblockDelays;
	HeatParameters::OptionalInterval leader5UnblockDelays;
	HeatParameters::OptionalInterval leader7UnblockDelays;

	// Conversions
	float baseSpeedThreshold = racerSpeedThresholds.current / 3.6f; // metres / second
	float jerkSpeedThreshold = baseSpeedThreshold * .625f;





	// StrategyManager class ------------------------------------------------------------------------------------------------------------------------

	class StrategyManager : public PursuitFeatures::PursuitReaction
	{
		using PursuitCache = PursuitFeatures::PursuitCache;



	private:

		int& numStrategyVehicles = *((int*)(this->pursuit + 0x18C));

		const float pursuitStartTimestamp = PursuitFeatures::simulationTime;

		const int&     pursuitStatus  = *((int*)(this->pursuit + 0x218));
		const bool&    isJerk         = *((bool*)(this->pursuit + 0x238));
		const address& heavyStrategy  = *((address*)(this->pursuit + 0x194));
		const address& leaderStrategy = *((address*)(this->pursuit + 0x198));

		address rigidBodyOfTarget      = 0x0;
		bool    watchingHeavyStrategy3 = false;

		PursuitFeatures::IntervalTimer unblockTimer;

		HashContainers::AddressSet vehiclesOfCurrentStrategy;
		HashContainers::AddressSet vehiclesOfUnblockedHeavy3;

		inline static const auto ClearSupportRequest = (void (__thiscall*)(address))0x42BCF0;

		inline static constexpr size_t cacheIndex = 2;


		void UpdateNumStrategyVehicles() const
		{
			this->numStrategyVehicles = (int)(this->vehiclesOfCurrentStrategy.size());
		}


		void StopUnblockTimer()
		{
			if (not this->unblockTimer.IsSet()) return;

			this->unblockTimer.Stop();

			if (this->watchingHeavyStrategy3)
			{
				this->watchingHeavyStrategy3 = false;
				this->vehiclesOfUnblockedHeavy3.merge(this->vehiclesOfCurrentStrategy);
			}
			else this->vehiclesOfCurrentStrategy.clear();

			this->UpdateNumStrategyVehicles();
		}


		void StartUnblockTimer()
		{
			if (this->unblockTimer.IsSet()) return;

			this->unblockTimer.Start();

			if constexpr (Globals::loggingEnabled)
			{
				if (this->unblockTimer.IsIntervalEnabled())
					Globals::logger.Log(this->pursuit, "[STR] Unblocking in", this->unblockTimer.GetLength());

				else
					Globals::logger.Log(this->pursuit, "[STR] Strategy blocking fully");
			}
		}


		void CheckUnblockTimer() const
		{
			if (not this->unblockTimer.HasExpired()) return;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[STR] Unblocking active Strategy");

			this->ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer
		}


		static void MakeVehiclesBail(HashContainers::AddressSet& copVehicles)
		{
			for (const address copVehicle : copVehicles)
				PursuitFeatures::MakeVehicleBail(copVehicle);

			copVehicles.clear();
		}


		void RetrieveRigidBodyOfTarget()
		{
			const address pursuitTarget = *((address*)(this->pursuit + 0x74));
			const address physicsObject = *((address*)(pursuitTarget + 0x1C));

			this->rigidBodyOfTarget = (physicsObject) ? *((address*)(physicsObject + 0x4C)) : 0x0;

			if constexpr (Globals::loggingEnabled)
			{
				if (not this->rigidBodyOfTarget)
					Globals::logger.Log("WARNING: [STR] Invalid PhysicsObject for target in", this->pursuit);
			}
		}


		bool IsSpeedOfTargetBelowThreshold() const
		{
			if (not this->rigidBodyOfTarget) return false;

			static const auto GetSpeedXZ = (float (__thiscall*)(address))0x6711F0;

			return (GetSpeedXZ(this->rigidBodyOfTarget) < ((this->isJerk) ? jerkSpeedThreshold : baseSpeedThreshold));
		}


		bool IsSearchModeActive() const
		{
			return (this->pursuitStatus == 2);
		}


		void CheckHeavyStrategy3Vehicles()
		{
			if ((not this->watchingHeavyStrategy3) and this->vehiclesOfUnblockedHeavy3.empty()) return;

			if (this->IsSpeedOfTargetBelowThreshold() or this->IsSearchModeActive())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[STR] Forcing HeavyStrategy3 vehicles to bail");

				this->MakeVehiclesBail(this->vehiclesOfUnblockedHeavy3);

				if (this->watchingHeavyStrategy3)
				{
					this->MakeVehiclesBail(this->vehiclesOfCurrentStrategy);
					this->ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer
				}
			}
		}
		
		

	public:

		explicit StrategyManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "StrategyManager");
			
			PursuitCache::SetPointer<this->cacheIndex>(this->pursuit, this);
		}


		~StrategyManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "StrategyManager");

			PursuitCache::ClearPointer<this->cacheIndex>(this->pursuit, this);
		}


		void UpdateOnGameplay() override
		{
			this->CheckHeavyStrategy3Vehicles();
			this->CheckUnblockTimer();
		}


		void UpdateOncePerPursuit() override
		{
			this->RetrieveRigidBodyOfTarget();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (not ((copLabel == CopLabel::HEAVY) or (copLabel == CopLabel::LEADER))) return;

			if (this->unblockTimer.IsSet())
			{
				this->vehiclesOfCurrentStrategy.insert(copVehicle);
				this->UpdateNumStrategyVehicles();
			}
			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [STR] New vehicle", copVehicle, "without Strategy in", this->pursuit);
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (not ((copLabel == CopLabel::HEAVY) or (copLabel == CopLabel::LEADER))) return;

			if (this->vehiclesOfCurrentStrategy.erase(copVehicle))
			{
				if (this->vehiclesOfCurrentStrategy.empty())
					this->ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer

				else
					this->UpdateNumStrategyVehicles();
			}
			else if (copLabel == CopLabel::HEAVY)
				this->vehiclesOfUnblockedHeavy3.erase(copVehicle);
		}


		static void __fastcall WatchHeavyStrategy(const address pursuit)
		{
			const auto manager = PursuitCache::GetPointer<StrategyManager::cacheIndex, StrategyManager>(pursuit);

			if (not manager) return;

			manager->StopUnblockTimer();

			if (not manager->heavyStrategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Invalid HeavyStrategy pointer in", pursuit);

				return;
			}

			const int strategyID = *((int*)(manager->heavyStrategy));

			switch (strategyID)
			{
			case 3:
				manager->watchingHeavyStrategy3 = true;
				manager->unblockTimer.LoadInterval(heavy3UnblockDelays);
				break;

			case 4:
				manager->unblockTimer.LoadInterval(heavy4UnblockDelays);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Unknown HeavyStrategy", strategyID, "in", pursuit);

				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[STR] Watching HeavyStrategy", strategyID);

			manager->StartUnblockTimer();
		}


		static void __fastcall WatchLeaderStrategy(const address pursuit)
		{
			const auto manager = PursuitCache::GetPointer<StrategyManager::cacheIndex, StrategyManager>(pursuit);

			if (not manager) return;

			manager->StopUnblockTimer();

			if (not manager->leaderStrategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Invalid LeaderStrategy pointer in", pursuit);

				return;
			}

			const int strategyID = *((int*)(manager->leaderStrategy));

			switch (strategyID)
			{
			case 5:
				manager->unblockTimer.LoadInterval(leader5UnblockDelays);
				break;

			case 7:
				manager->unblockTimer.LoadInterval(leader7UnblockDelays);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Unknown LeaderStrategy", strategyID, "in", pursuit);

				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[STR] Watching LeaderStrategy", strategyID);

			manager->StartUnblockTimer();
		}


		static void __fastcall ClearWatchedStrategy(const address pursuit)
		{
			const auto manager = PursuitCache::GetPointer<StrategyManager::cacheIndex, StrategyManager>(pursuit);

			if (manager)
			{
				manager->StopUnblockTimer();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[STR] Active Strategy cleared");
			}
		}


		static float __fastcall GetTruePursuitLength(const address pursuit)
		{
			const auto manager = PursuitCache::GetPointer<StrategyManager::cacheIndex, StrategyManager>(pursuit);
			return (manager) ? (PursuitFeatures::simulationTime - manager->pursuitStartTimestamp) : 0.f;
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address goalResetEntrance = 0x42B475;
	constexpr address goalResetExit     = 0x42B47A;

	__declspec(naked) void GoalReset()
	{
		static constexpr address goalResetSkip = 0x42B48E;

		__asm
		{
			mov ebx, dword ptr [esp + 0x14]

			cmp ebx, 0x443EDA // duration expired
			je reset

			cmp ebx, 0x43311B // pursuit destructor
			je reset

			cmp ebx, 0x4431F4 // pursuit constructor
			jne skip          // do not reset

			reset:
			push ecx
			mov ebx, esp
			push 0x0

			jmp dword ptr goalResetExit

			skip:
			jmp dword ptr goalResetSkip
		}
	}



	constexpr address clearRequestEntrance = 0x42B431;
	constexpr address clearRequestExit     = 0x42B436;

	__declspec(naked) void ClearRequest()
	{
		__asm
		{
			mov dword ptr [ebp + 0x78], edi
			mov eax, dword ptr [esp + 0x14]

			cmp eax, 0x43311B // pursuit destructor
			je conclusion

			cmp eax, 0x4431F4 // pursuit constructor
			je conclusion

			push ecx

			lea ecx, dword ptr [ebp + 0x78 - 0x20C]
			call StrategyManager::ClearWatchedStrategy // ecx: pursuit

			pop ecx

			conclusion:
			test cl, cl

			jmp dword ptr clearRequestExit
		}
	}



	constexpr address leaderStrategyEntrance = 0x41F706;
	constexpr address leaderStrategyExit     = 0x41F70D;

	__declspec(naked) void LeaderStrategy()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x90]
			call StrategyManager::WatchLeaderStrategy // ecx: pursuit

			// Execute original code and resume
			mov eax, dword ptr [esp + 0x94]

			jmp dword ptr leaderStrategyExit
		}
	}



	constexpr address heavyStrategy3Entrance = 0x41F38F;
	constexpr address heavyStrategy3Exit     = 0x41F396;

	__declspec(naked) void HeavyStrategy3()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0xC4]
			call StrategyManager::WatchHeavyStrategy // ecx: pursuit

			// Execute original code and resume
			mov eax, dword ptr [esp + 0xC8]

			jmp dword ptr heavyStrategy3Exit
		}
	}



	constexpr address heavyStrategy4Entrance = 0x43E7DF;
	constexpr address heavyStrategy4Exit     = 0x43E7E8;

	__declspec(naked) void HeavyStrategy4()
	{
		__asm
		{
			mov ecx, ebx
			call StrategyManager::WatchHeavyStrategy // ecx: pursuit

			// Execute original code and resume
			mov ecx, dword ptr [esi]
			mov dword ptr [esi + 0x78], 0x2

			jmp dword ptr heavyStrategy4Exit
		}
	}



	constexpr address minStrategyDelayEntrance = 0x4196F2;
	constexpr address minStrategyDelayExit     = 0x4196FA;

	__declspec(naked) void MinStrategyDelay()
	{
		__asm
		{
			push eax

			mov ecx, esi
			call StrategyManager::GetTruePursuitLength // ecx: pursuit

			pop eax
			fld dword ptr [eax]
			fcompp

			jmp dword ptr minStrategyDelayExit
		}
	}



	constexpr address minRoadblockDelayEntrance = 0x41950C;
	constexpr address minRoadblockDelayExit     = 0x419514;

	__declspec(naked) void MinRoadblockDelay()
	{
		__asm
		{
			push eax

			mov ecx, esi
			call StrategyManager::GetTruePursuitLength // ecx: pursuit

			pop eax
			fld dword ptr [eax]
			fcompp

			jmp dword ptr minRoadblockDelayExit
		}
	}



	constexpr address crossPriorityDelayEntrance = 0x41973D;
	constexpr address crossPriorityDelayExit     = 0x419746;

	__declspec(naked) void CrossPriorityDelay()
	{
		__asm
		{
			push ecx

			mov ecx, esi
			call StrategyManager::GetTruePursuitLength // ecx: pursuit

			pop ecx
			fld dword ptr [ecx + 0x10]
			fcompp

			jmp dword ptr crossPriorityDelayExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini");

		// Heat parameters
		HeatParameters::Parse<float>(parser, "Heavy3:Fleeing", {racerSpeedThresholds, 0.f});

		HeatParameters::Parse<>(parser, "Heavy3:Unblocking",  heavy3UnblockDelays);
		HeatParameters::Parse<>(parser, "Heavy4:Unblocking",  heavy4UnblockDelays);
		HeatParameters::Parse<>(parser, "Leader5:Unblocking", leader5UnblockDelays);
		HeatParameters::Parse<>(parser, "Leader7:Unblocking", leader7UnblockDelays);

		// Code modifications 
		MemoryTools::Write<byte>   (0xE9,  {0x44384A}); // skip vanilla "CollapseSpeed" HeavyStrategy check
		MemoryTools::Write<address>(0x2A3, {0x44384B});
		
		MemoryTools::MakeRangeNOP(0x4240BD, 0x4240C3); // OnAttached increment
		MemoryTools::MakeRangeNOP(0x42B717, 0x42B72E); // OnDetached decrement

		MemoryTools::MakeRangeJMP(GoalReset,      goalResetEntrance,      goalResetExit);
		MemoryTools::MakeRangeJMP(ClearRequest,   clearRequestEntrance,   clearRequestExit);
		MemoryTools::MakeRangeJMP(LeaderStrategy, leaderStrategyEntrance, leaderStrategyExit);
		MemoryTools::MakeRangeJMP(HeavyStrategy3, heavyStrategy3Entrance, heavyStrategy3Exit);
		MemoryTools::MakeRangeJMP(HeavyStrategy4, heavyStrategy4Entrance, heavyStrategy4Exit);

		MemoryTools::MakeRangeJMP(MinStrategyDelay,   minStrategyDelayEntrance,   minStrategyDelayExit);
		MemoryTools::MakeRangeJMP(MinRoadblockDelay,  minRoadblockDelayEntrance,  minRoadblockDelayExit);
		MemoryTools::MakeRangeJMP(CrossPriorityDelay, crossPriorityDelayEntrance, crossPriorityDelayExit);
		
		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log          ("    HEAT [STR] StrategyOverrides");
		Globals::logger.LogLongIndent("racerSpeedThreshold     ", racerSpeedThresholds.current);

		heavy3UnblockDelays .Log("heavy3UnblockDelay      ");
		heavy4UnblockDelays .Log("heavy4UnblockDelay      ");
		leader5UnblockDelays.Log("leader5UnblockDelay     ");
		leader7UnblockDelays.Log("leader7UnblockDelay     ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		racerSpeedThresholds.SetToHeat(isRacing, heatLevel);

		baseSpeedThreshold = racerSpeedThresholds.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;

		heavy3UnblockDelays .SetToHeat(isRacing, heatLevel);
		heavy4UnblockDelays .SetToHeat(isRacing, heatLevel);
		leader5UnblockDelays.SetToHeat(isRacing, heatLevel);
		leader7UnblockDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}