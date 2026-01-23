#pragma once

#include "Globals.h"
#include "MemoryTools.h"
#include "HashContainers.h"
#include "HeatParameters.h"

#include "GeneralSettings.h"

#include "PursuitFeatures.h"
#include "CopFleeOverrides.h"



namespace StrategyOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Interval<int> numVehiclesPerHeavy3s(2, 2);

	HeatParameters::OptionalInterval<float> heavy3UnblockDelays;  // seconds
	HeatParameters::OptionalInterval<float> heavy4UnblockDelays;  // seconds
	HeatParameters::OptionalInterval<float> leader5UnblockDelays; // seconds
	HeatParameters::OptionalInterval<float> leader7UnblockDelays; // seconds

	// Code caves
	constexpr size_t maxNumVehiclesPerHeavy4 = 6;

	std::vector<float> spawnVectorStackA;
	std::vector<float> spawnVectorStackB;

	float* spawnVectorsA = nullptr;
	float* spawnVectorsB = nullptr;





	// StrategyManager class ------------------------------------------------------------------------------------------------------------------------

	class StrategyManager : public PursuitFeatures::PursuitReaction
	{
	private:

		const float pursuitStartTimestamp = Globals::simulationTime;

		size_t nextHeavy3Count = 2;

		int& numStrategyVehicles = *reinterpret_cast<int*>(this->pursuit + 0x18C);
	
		const address& heavyStrategy  = *reinterpret_cast<address*>(this->pursuit + 0x194);
		const address& leaderStrategy = *reinterpret_cast<address*>(this->pursuit + 0x198);

		PursuitFeatures::IntervalTimer unblockTimer;
		HashContainers ::AddressSet    vehiclesOfCurrentStrategy;

		inline static HashContainers::AddressMap<StrategyManager*> pursuitToManager;


		void UpdateNextHeavy3Count()
		{
			this->nextHeavy3Count = static_cast<size_t>(numVehiclesPerHeavy3s.GetRandomValue());

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[STR] Next Heavy3 count:", static_cast<int>(this->nextHeavy3Count));
		}


		void UpdateNumStrategyVehicles() const
		{
			this->numStrategyVehicles = static_cast<int>(this->vehiclesOfCurrentStrategy.size());
		}


		void StopUnblockTimer()
		{
			if (not this->unblockTimer.IsSet()) return;

			this->unblockTimer.Stop();

			this->vehiclesOfCurrentStrategy.clear();
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

			Globals::ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer
		}


		static StrategyManager* FindManager(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
				return foundManager->second;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [STR] No manager for pursuit", pursuit);

			return nullptr;
		}



	public:

		explicit StrategyManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('+', this, "StrategyManager");
			
			this->pursuitToManager.try_emplace(this->pursuit, this);
		}


		~StrategyManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.LogLongIndent('-', this, "StrategyManager");

			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnGameplay() override
		{
			this->CheckUnblockTimer();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateNextHeavy3Count();
		}


		void UpdateOncePerPursuit() override
		{
			this->UpdateNextHeavy3Count();
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
					Globals::ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer

				else
					this->UpdateNumStrategyVehicles();
			}
		}


		static void __fastcall WatchHeavyStrategy(const address pursuit)
		{
			StrategyManager* const manager = StrategyManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->StopUnblockTimer();

			if (not manager->heavyStrategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Invalid HeavyStrategy pointer in", pursuit);

				return; // should never happen
			}

			const int strategyID = *reinterpret_cast<int*>(manager->heavyStrategy);

			switch (strategyID)
			{
			case 3:
				manager->unblockTimer.LoadInterval(heavy3UnblockDelays);
				manager->UpdateNextHeavy3Count();
				break;

			case 4:
				manager->unblockTimer.LoadInterval(heavy4UnblockDelays);
				break;

			default:
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Unknown HeavyStrategy", strategyID, "in", pursuit);

				return; // should never happen
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[STR] Watching HeavyStrategy", strategyID);

			manager->StartUnblockTimer();
		}


		static void __fastcall WatchLeaderStrategy(const address pursuit)
		{
			StrategyManager* const manager = StrategyManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->StopUnblockTimer();

			if (not manager->leaderStrategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [STR] Invalid LeaderStrategy pointer in", pursuit);

				return; // should never happen
			}

			const int strategyID = *reinterpret_cast<int*>(manager->leaderStrategy);

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

				return; // should never happen
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[STR] Watching LeaderStrategy", strategyID);

			manager->StartUnblockTimer();
		}


		static void __fastcall ClearWatchedStrategy(const address pursuit)
		{
			StrategyManager* const manager = StrategyManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->StopUnblockTimer();

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(pursuit, "[STR] Active Strategy cleared");
		}


		static float __fastcall GetFullPursuitLength(const address pursuit)
		{
			const StrategyManager* const manager = StrategyManager::FindManager(pursuit);
			return (manager) ? (Globals::simulationTime - manager->pursuitStartTimestamp) : 0.f;
		}


		static size_t __fastcall GetNextHeavy3Count(const address pursuit)
		{
			const StrategyManager* const manager = StrategyManager::FindManager(pursuit);
			return (manager) ? manager->nextHeavy3Count : 2;
		}
	};





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address goalResetEntrance = 0x42B475;
	constexpr address goalResetExit     = 0x42B47A;

	// Prevents some Strategy resets from cancelling Strategy goals
	__declspec(naked) void GoalReset()
	{
		static constexpr address goalResetSkip = 0x42B48E;

		__asm
		{
			mov ebx, dword ptr [esp + 0x14] // caller

			cmp ebx, 0x443EDA // duration expired
			je reset          // allow goal reset

			cmp ebx, 0x43311B // pursuit destructor
			je reset          // allow goal reset

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



	constexpr address heavyGoalEntrance = 0x41F21F;
	constexpr address heavyGoalExit     = 0x41F226;

	// Ensures HeavyStrategy vehicles have the right goal
	__declspec(naked) void HeavyGoal()
	{
		__asm
		{
			mov edx, dword ptr [esp + 0x10]
			mov eax, dword ptr [edx]

			jmp dword ptr heavyGoalExit
		}
	}



	constexpr address heavy3CountEntrance = 0x41F170;
	constexpr address heavy3CountExit     = 0x41F17D;

	// Sets how many HeavyStrategy 3 vehicles spawn per request
	__declspec(naked) void Heavy3Count()
	{
		__asm
		{	
			mov ebx, ecx

			lea ecx, dword ptr [esi - 0x194]
			call StrategyManager::GetNextHeavy3Count // ecx: pursuit
			mov ebp, eax

			mov ecx, ebx

			// Execute original code and resume
			mov dword ptr [esp + 0x10], 0x8EB1BC

			jmp dword ptr heavy3CountExit
		}
	}



	constexpr address heavy3SetupEntrance = 0x41F314;
	constexpr address heavy3SetupExit     = 0x41F31C;

	// Makes the game use mod-allocated spawn vectors
	__declspec(naked) void Heavy3Setup()
	{
		__asm
		{
			mov esi, dword ptr spawnVectorsA
			mov edi, dword ptr spawnVectorsB

			add esi, ebx // vector offset
			add edi, ebx // vector offset

			jmp dword ptr heavy3SetupExit
		}
	}



	constexpr address clearRequestEntrance = 0x42B431;
	constexpr address clearRequestExit     = 0x42B436;

	// Notifies Strategy managers of cleared requests
	__declspec(naked) void ClearRequest()
	{
		__asm
		{
			mov dword ptr [ebp + 0x78], edi
			mov eax, dword ptr [esp + 0x14] // caller

			cmp eax, 0x43311B // pursuit destructor
			je conclusion     // skip manager notification

			cmp eax, 0x4431F4 // pursuit constructor
			je conclusion     // skip manager notification

			push ecx

			lea ecx, dword ptr [ebp + 0x78 - 0x20C]
			call StrategyManager::ClearWatchedStrategy // ecx: pursuit

			pop ecx

			conclusion:
			test cl, cl

			jmp dword ptr clearRequestExit
		}
	}



	constexpr address heavy3PositionEntrance = 0x41F3C6;
	constexpr address heavy3PositionExit     = 0x41F40A;

	// Ensures proper accessing of mod-allocated spawn vectors
	__declspec(naked) void Heavy3Position()
	{
		__asm
		{
			mov edx, dword ptr spawnVectorsA
			xor ecx, ecx
			add edx, ebp // vector offset

			mov edi, eax

			negation:
			fld dword ptr [edx + ecx * 0x4]
			fchs // original code multiplies by -1.0
			fstp dword ptr [edx + ecx * 0x4]

			inc ecx
			cmp ecx, 0x4
			jl negation // elements remaining

			mov eax, dword ptr spawnVectorsB
			mov ecx, edi
			add eax, ebp // vector offset

			push edx
			push eax

			jmp dword ptr heavy3PositionExit
		}
	}



	constexpr address leaderStrategyEntrance = 0x41F706;
	constexpr address leaderStrategyExit     = 0x41F70D;

	// Notifies Strategy managers of new LeaderStrategy requests
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

	// Notifies Strategy managers of new HeavyStrategy3 requests
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

	// Notifies Strategy managers of new HeavyStrategy4 requests
	__declspec(naked) void HeavyStrategy4()
	{
		__asm
		{
			mov ecx, ebx
			call StrategyManager::WatchHeavyStrategy // ecx: pursuit

			// Execute original code and resume
			mov ecx, dword ptr [esi]
			mov dword ptr [esi + 0x78], 0x2 // Strategy request status

			jmp dword ptr heavyStrategy4Exit
		}
	}



	constexpr address minStrategyDelayEntrance = 0x4196F4;
	constexpr address minStrategyDelayExit     = 0x4196FA;

	// Ensures cops can request available Strategies in races
	__declspec(naked) void MinStrategyDelay()
	{
		__asm
		{
			mov ecx, esi
			call StrategyManager::GetFullPursuitLength // ecx: pursuit
			fxch st(1)
			fcompp

			jmp dword ptr minStrategyDelayExit
		}
	}



	constexpr address minRoadblockDelayEntrance = 0x41950E;
	constexpr address minRoadblockDelayExit     = 0x419514;

	// Ensures cops can request non-Strategy roadblocks in races
	__declspec(naked) void MinRoadblockDelay()
	{
		__asm
		{
			mov ecx, esi
			call StrategyManager::GetFullPursuitLength // ecx: pursuit
			fxch st(1)
			fcompp

			jmp dword ptr minRoadblockDelayExit
		}
	}



	constexpr address crossPriorityDelayEntrance = 0x419740;
	constexpr address crossPriorityDelayExit     = 0x419746;

	// Ensures cops can issue priority requests for Cross in races
	__declspec(naked) void CrossPriorityDelay()
	{
		__asm
		{
			push ecx

			mov ecx, esi
			call StrategyManager::GetFullPursuitLength // ecx: pursuit
			fxch st(1)
			fcompp

			pop ecx

			jmp dword ptr crossPriorityDelayExit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini");

		// Heat parameters
		HeatParameters::Parse<int>(parser, "Heavy3:Count", {numVehiclesPerHeavy3s, 1});

		HeatParameters::ParseOptional<float>(parser, "Heavy3:Unblocking",  {heavy3UnblockDelays , 1.f});
		HeatParameters::ParseOptional<float>(parser, "Heavy4:Unblocking",  {heavy4UnblockDelays , 1.f});
		HeatParameters::ParseOptional<float>(parser, "Leader5:Unblocking", {leader5UnblockDelays, 1.f});
		HeatParameters::ParseOptional<float>(parser, "Leader7:Unblocking", {leader7UnblockDelays, 1.f});

		// Stack replacements for removal of HeavyStrategy 3 limit
		const size_t vectorStackSize = 4 * std::max<size_t>(numVehiclesPerHeavy3s.GetMaximum(), 5); // floats

		spawnVectorStackA.resize(vectorStackSize);
		spawnVectorStackB.resize(vectorStackSize);

		spawnVectorsA = spawnVectorStackA.data();
		spawnVectorsB = spawnVectorStackB.data();

		// Code modifications 
		MemoryTools::Write<size_t>(maxNumVehiclesPerHeavy4, {0x41F188}); // spawn limit for HeavyStrategy 4
		MemoryTools::Write<byte>  (maxNumVehiclesPerHeavy4, {0x43E7CD}); // car budget

		MemoryTools::Write<byte>   (0xE9,  {0x44384A}); // skip vanilla "CollapseSpeed" HeavyStrategy check
		MemoryTools::Write<address>(0x2A3, {0x44384B});

		MemoryTools::MakeRangeNOP(0x4240BD, 0x4240C3); // OnAttached increment
		MemoryTools::MakeRangeNOP(0x42B717, 0x42B72E); // OnDetached decrement

		MemoryTools::MakeRangeJMP(GoalReset,      goalResetEntrance,      goalResetExit);
		MemoryTools::MakeRangeJMP(HeavyGoal,      heavyGoalEntrance,      heavyGoalExit);
		MemoryTools::MakeRangeJMP(Heavy3Count,    heavy3CountEntrance,    heavy3CountExit);
		MemoryTools::MakeRangeJMP(Heavy3Setup,    heavy3SetupEntrance,    heavy3SetupExit);
		MemoryTools::MakeRangeJMP(ClearRequest,   clearRequestEntrance,   clearRequestExit);
		MemoryTools::MakeRangeJMP(Heavy3Position, heavy3PositionEntrance, heavy3PositionExit);
		MemoryTools::MakeRangeJMP(LeaderStrategy, leaderStrategyEntrance, leaderStrategyExit);
		MemoryTools::MakeRangeJMP(HeavyStrategy3, heavyStrategy3Entrance, heavyStrategy3Exit);
		MemoryTools::MakeRangeJMP(HeavyStrategy4, heavyStrategy4Entrance, heavyStrategy4Exit);

		if (not GeneralSettings::trackPursuitLength)
		{
			MemoryTools::MakeRangeJMP(MinStrategyDelay,   minStrategyDelayEntrance,   minStrategyDelayExit);
			MemoryTools::MakeRangeJMP(MinRoadblockDelay,  minRoadblockDelayEntrance,  minRoadblockDelayExit);
			MemoryTools::MakeRangeJMP(CrossPriorityDelay, crossPriorityDelayEntrance, crossPriorityDelayExit);
		}

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [STR] StrategyOverrides");

		numVehiclesPerHeavy3s.Log("numVehiclesPerHeavy3    ");

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

		numVehiclesPerHeavy3s.SetToHeat(isRacing, heatLevel);

		heavy3UnblockDelays .SetToHeat(isRacing, heatLevel);
		heavy4UnblockDelays .SetToHeat(isRacing, heatLevel);
		leader5UnblockDelays.SetToHeat(isRacing, heatLevel);
		leader7UnblockDelays.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}