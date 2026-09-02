#pragma once

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"
#include "../../Common/HeatParameters.hpp"

#include "../../Utilities/MemoryTools.hpp"

#include "../Basic/GeneralSettings.hpp"

#include "PursuitFeatures.hpp"



namespace StrategyOverrides
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[STG]";
	constexpr Globals::LogLiteral logName = "StrategyOverrides";

	// Spawn limits
	constexpr size_t maxNumVehiclesPerHeavy3 = 20; // cars
	constexpr size_t maxNumVehiclesPerHeavy4 = 6;  // cars

	// Aliases
	using Vector4Ds = float[maxNumVehiclesPerHeavy3][4]; // C-style for ASM

	// Heat parameters
	constinit HEAT_PARAMETER_INTERVAL(int, numVehiclesPerHeavy3s, 2, 2, {1, maxNumVehiclesPerHeavy3}); // cars

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, heavy3UnblockDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, heavy4UnblockDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader5UnblockDelay, {1.f}); // seconds

	constinit OPTIONAL_HEAT_PARAMETER_INTERVAL(float, leader7UnblockDelay, {1.f}); // seconds

	// Assembly detours
	constinit Vector4Ds heavy3SpawnPositions; // C-style for ASM
	constinit Vector4Ds heavy3InitialVectors; // C-style for ASM





	// StrategyManager class ------------------------------------------------------------------------------------------------------------------------

	class StrategyManager : public PursuitFeatures::Reaction, public PursuitFeatures::Searchable<StrategyManager>
	{
	private: // members

		int nextHeavy3Count = 2; // cars

		const float pursuitStartTimestamp = Globals::simulationTime; // seconds

		int& numStrategyVehicles = AsReference<int>(this->pursuit + 0x18C); // cars
	
		const address& heavyStrategy  = AsReference<address>(this->pursuit + 0x194);
		const address& leaderStrategy = AsReference<address>(this->pursuit + 0x198);

		PursuitFeatures::IntervalTimer unblockTimer;

		ModContainers::AddressSet vehiclesOfCurrentStrategy;

		inline static constexpr Globals::LogLiteral name = "StrategyManager";


	private: // methods

		void UpdateNextHeavy3Count()
		{
			this->nextHeavy3Count = numVehiclesPerHeavy3s.GetRandomValue();

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Next HeavyStrategy 3 count:", this->nextHeavy3Count);
		}


		void UpdateNumStrategyVehicles() const
		{
			this->numStrategyVehicles = static_cast<int>(this->vehiclesOfCurrentStrategy.size());
		}


		void StopUnblockTimer()
		{
			if (not this->unblockTimer.HasStartTimestamp()) return;

			this->unblockTimer.ClearStartTimestamp();

			this->vehiclesOfCurrentStrategy.clear();
			this->UpdateNumStrategyVehicles();
		}


		void StartUnblockTimer()
		{
			if (this->unblockTimer.HasStartTimestamp()) return;

			this->unblockTimer.SetStartTimestamp();

			if constexpr (Globals::loggingEnabled)
			{
				Globals::LogFull(this->pursuit, logTag, "New unblock timer");

				this->unblockTimer.Log("Unblocking");
			}
		}


		void CheckUnblockTimer() const
		{
			if (not this->unblockTimer.HasExpired()) return;

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(this->pursuit, logTag, "Unblocking active Strategy");

			Globals::ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer
		}


		[[nodiscard]] static bool IsStrategyCop(const CopLabel copLabel)
		{
			switch (copLabel)
			{
				case CopLabel::HEAVY:
				case CopLabel::LEADER:
					return true;
			}

			return false;
		}


	public: // members

		inline static constinit const bool& isEnabled = anyFeatureEnabled;


	public: // methods

		explicit StrategyManager(const address pursuit) : PursuitFeatures::Reaction(pursuit)
		{
			this->vehiclesOfCurrentStrategy.reserve(10);

			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('+', this, this->name);
		}


		~StrategyManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::LogPlain('-', this, this->name);
		}


		void ReactToGameplay() override
		{
			this->CheckUnblockTimer();
		}


		void ReactToHeatStateUpdate() override
		{
			this->UpdateNextHeavy3Count();
		}


		void ReactToPursuitStartWithDelay() override
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
			if (not this->IsStrategyCop(copLabel)) return;

			if (not this->unblockTimer.HasStartTimestamp())
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "New vehicle", copVehicle, "without Strategy in", this->pursuit);

				ASSERT_UNREACHABLE_THEN(return);
			}

			this->vehiclesOfCurrentStrategy.insert(copVehicle);
			this->UpdateNumStrategyVehicles();
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			if (not this->IsStrategyCop(copLabel))                     return;
			if (not this->vehiclesOfCurrentStrategy.erase(copVehicle)) return;

			if (this->vehiclesOfCurrentStrategy.empty())
				Globals::ClearSupportRequest(this->pursuit); // also calls StopUnblockTimer

			else this->UpdateNumStrategyVehicles();
		}


		static void __fastcall WatchHeavyStrategy(const address pursuit)
		{
			auto* const manager = StrategyManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->StopUnblockTimer();

			if (not manager->heavyStrategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid HeavyStrategy pointer in", pursuit);

				ASSERT_UNREACHABLE_THEN(return);
			}

			const int strategyID = AsReference<int>(manager->heavyStrategy);

			switch (strategyID)
			{
			case 3: // ramming SUVs
				manager->UpdateNextHeavy3Count();
				manager->unblockTimer.LoadInterval(heavy3UnblockDelay);
				break;

			case 4: // SUV roadblock
				manager->unblockTimer.LoadInterval(heavy4UnblockDelay);
				break;

			default:
				manager->unblockTimer.DisableInterval();

				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "HeavyStrategy", strategyID, "in", pursuit);
			}

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "Watching HeavyStrategy", strategyID);

			manager->StartUnblockTimer();
		}


		static void __fastcall WatchLeaderStrategy(const address pursuit)
		{
			auto* const manager = StrategyManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->StopUnblockTimer();

			if (not manager->leaderStrategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "Invalid LeaderStrategy pointer in", pursuit);

				ASSERT_UNREACHABLE_THEN(return);
			}

			const int strategyID = AsReference<int>(manager->leaderStrategy);

			switch (strategyID)
			{
			case 5: // Cross only
				manager->unblockTimer.LoadInterval(leader5UnblockDelay);
				break;

			case 7: // Cross with henchmen
				manager->unblockTimer.LoadInterval(leader7UnblockDelay);
				break;

			default:
				manager->unblockTimer.DisableInterval();

				if constexpr (Globals::loggingEnabled)
					Globals::LogWarning(logTag, "LeaderStrategy", strategyID, "in", pursuit);
			}

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "Watching LeaderStrategy", strategyID);

			manager->StartUnblockTimer();
		}


		static void __fastcall ClearWatchedStrategy(const address pursuit)
		{
			auto* const manager = StrategyManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return);

			manager->StopUnblockTimer();

			if constexpr (Globals::loggingEnabled)
				Globals::LogFull(pursuit, logTag, "Active Strategy cleared");
		}


		[[nodiscard]] static float __fastcall GetFullPursuitLength(const address pursuit)
		{
			const auto* const manager = StrategyManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return 0.f);

			return Globals::simulationTime - manager->pursuitStartTimestamp;
		}


		[[nodiscard]] static int __fastcall GetNextHeavy3Count(const address pursuit)
		{
			const auto* const manager = StrategyManager::FindInstance(pursuit);
			ASSERT_CONDITION_THEN_IF_FALSE(manager, return 2);

			return manager->nextHeavy3Count;
		}
	};





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Prevents some Strategy resets from cancelling Strategy goals
	ASSEMBLY_DETOUR(GoalReset, /* begin = */ 0x42B475, /* end = */ 0x42B47A)
	{
		static constexpr address retainExit = 0x42B48E;

		__asm
		{
			mov ebx, dword ptr [esp + 0x14] // caller

			cmp ebx, 0x443EDA // duration expired
			je reset          // allow goal reset

			cmp ebx, 0x43311B // pursuit destructor
			je reset          // allow goal reset

			cmp ebx, 0x4431F4 // pursuit constructor
			jne retain        // do not reset

			reset:
			push ecx
			mov ebx, esp
			push 0

			EXIT_ASSEMBLY_DETOUR(GoalReset)

			retain:
			jmp dword ptr [retainExit]
		}
	}



	// Ensures HeavyStrategy vehicles have the right goal
	ASSEMBLY_DETOUR(HeavyGoal, 0x41F21F, 0x41F226)
	{
		__asm
		{
			mov edx, dword ptr [esp + 0x10]
			mov eax, dword ptr [edx]

			EXIT_ASSEMBLY_DETOUR(HeavyGoal)
		}
	}



	// Sets how many HeavyStrategy 3 vehicles spawn per request
	ASSEMBLY_DETOUR(Heavy3Count, 0x41F170, 0x41F17D)
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

			EXIT_ASSEMBLY_DETOUR(Heavy3Count)
		}
	}



	// Makes the game use mod-allocated spawn vectors
	ASSEMBLY_DETOUR(Heavy3Setup, 0x41F314, 0x41F31C)
	{
		__asm
		{
			mov esi, offset heavy3SpawnPositions
			mov edi, offset heavy3InitialVectors

			add esi, ebx // vector offset
			add edi, ebx // vector offset

			EXIT_ASSEMBLY_DETOUR(Heavy3Setup)
		}
	}



	// Notifies Strategy managers of cleared requests
	ASSEMBLY_DETOUR(ClearRequest, 0x42B431, 0x42B436)
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

			EXIT_ASSEMBLY_DETOUR(ClearRequest)
		}
	}



	// Ensures proper accessing of mod-allocated spawn vectors
	ASSEMBLY_DETOUR(Heavy3Position, 0x41F3C6, 0x41F40A)
	{
		__asm
		{
			mov edx, offset heavy3SpawnPositions
			
			mov edi, eax
			add edx, ebp // vector offset

			mov ecx, 0x80000000 // sign-bit mask

			xor dword ptr [edx + 0x0], ecx
			xor dword ptr [edx + 0x4], ecx
			xor dword ptr [edx + 0x8], ecx
			xor dword ptr [edx + 0xC], ecx

			mov eax, offset heavy3InitialVectors

			mov ecx, edi
			add eax, ebp // vector offset

			push edx
			push eax

			EXIT_ASSEMBLY_DETOUR(Heavy3Position)
		}
	}



	// Notifies Strategy managers of new LeaderStrategy requests
	ASSEMBLY_DETOUR(LeaderStrategy, 0x41F706, 0x41F70D)
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x90]
			call StrategyManager::WatchLeaderStrategy // ecx: pursuit

			// Execute original code and resume
			mov eax, dword ptr [esp + 0x94]

			EXIT_ASSEMBLY_DETOUR(LeaderStrategy)
		}
	}



	// Notifies Strategy managers of new HeavyStrategy3 requests
	ASSEMBLY_DETOUR(HeavyStrategy3, 0x41F38F, 0x41F396)
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0xC4]
			call StrategyManager::WatchHeavyStrategy // ecx: pursuit

			// Execute original code and resume
			mov eax, dword ptr [esp + 0xC8]

			EXIT_ASSEMBLY_DETOUR(HeavyStrategy3)
		}
	}



	// Notifies Strategy managers of new HeavyStrategy4 requests
	ASSEMBLY_DETOUR(HeavyStrategy4, 0x43E7DF, 0x43E7E8)
	{
		__asm
		{
			mov ecx, ebx
			call StrategyManager::WatchHeavyStrategy // ecx: pursuit

			// Execute original code and resume
			mov ecx, dword ptr [esi]
			mov dword ptr [esi + 0x78], 2 // Strategy request status

			EXIT_ASSEMBLY_DETOUR(HeavyStrategy4)
		}
	}



	// Ensures cops can request available Strategies in races
	ASSEMBLY_DETOUR(MinStrategyDelay, 0x4196F4, 0x4196FA)
	{
		__asm
		{
			mov ecx, esi
			call StrategyManager::GetFullPursuitLength // ecx: pursuit
			fxch st(1)
			fcompp

			EXIT_ASSEMBLY_DETOUR(MinStrategyDelay)
		}
	}



	// Ensures cops can request non-Strategy roadblocks in races
	ASSEMBLY_DETOUR(MinRoadblockDelay, 0x41950E, 0x419514)
	{
		__asm
		{
			mov ecx, esi
			call StrategyManager::GetFullPursuitLength // ecx: pursuit
			fxch st(1)
			fcompp

			EXIT_ASSEMBLY_DETOUR(MinRoadblockDelay)
		}
	}



	// Ensures cops can issue priority requests for Cross in races
	ASSEMBLY_DETOUR(CrossPriorityDelay, 0x419740, 0x419746)
	{
		__asm
		{
			push ecx

			mov ecx, esi
			call StrategyManager::GetFullPursuitLength // ecx: pursuit
			fxch st(1)
			fcompp

			pop ecx

			EXIT_ASSEMBLY_DETOUR(CrossPriorityDelay)
		}
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		parser.ParseFile(Globals::pathAdvanced, Globals::fileStrategies);

		// Heat parameters
		HeatParameters::Extract(parser, "Heavy3:Count", numVehiclesPerHeavy3s);

		HeatParameters::Extract(parser, "Heavy3:Unblocking", heavy3UnblockDelay);

		HeatParameters::Extract(parser, "Heavy4:Unblocking", heavy4UnblockDelay);

		HeatParameters::Extract(parser, "Leader5:Unblocking", leader5UnblockDelay);

		HeatParameters::Extract(parser, "Leader7:Unblocking", leader7UnblockDelay);

		// Code modifications (general)
		MemoryTools::Write<size_t>(maxNumVehiclesPerHeavy4, {0x41F188}); // spawn limit for HeavyStrategy 4
		MemoryTools::Write<byte>  (maxNumVehiclesPerHeavy4, {0x43E7CD}); // car budget (actually unused)

		MemoryTools::Write<byte>   (0xE9,  {0x44384A}); // skip vanilla "CollapseSpeed" HeavyStrategy check
		MemoryTools::Write<address>(0x2A3, {0x44384B});

		MemoryTools::MakeRangeNOP<0x4240BD, 0x4240C3>(); // OnAttached increment
		MemoryTools::MakeRangeNOP<0x42B717, 0x42B72E>(); // OnDetached decrement

		PATCH_ASSEMBLY_DETOUR(GoalReset);
		PATCH_ASSEMBLY_DETOUR(HeavyGoal);
		PATCH_ASSEMBLY_DETOUR(Heavy3Count);
		PATCH_ASSEMBLY_DETOUR(Heavy3Setup);
		PATCH_ASSEMBLY_DETOUR(ClearRequest);
		PATCH_ASSEMBLY_DETOUR(Heavy3Position);
		PATCH_ASSEMBLY_DETOUR(LeaderStrategy);
		PATCH_ASSEMBLY_DETOUR(HeavyStrategy3);
		PATCH_ASSEMBLY_DETOUR(HeavyStrategy4);

		// Code modifications (conditional)
		if (not (GeneralSettings::anyFeatureEnabled and GeneralSettings::trackPursuitLength))
		{
			PATCH_ASSEMBLY_DETOUR(MinStrategyDelay);
			PATCH_ASSEMBLY_DETOUR(MinRoadblockDelay);
			PATCH_ASSEMBLY_DETOUR(CrossPriorityDelay);
		}

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}



	void SetToHeatState(const HeatParameters::HeatState state)
	{
		if (not anyFeatureEnabled) return;

		if constexpr (Globals::loggingEnabled)
			Globals::LogHeat(logTag, logName);

		numVehiclesPerHeavy3s.SetToHeatState(state);

		heavy3UnblockDelay.SetToHeatState(state);

		heavy4UnblockDelay.SetToHeatState(state);

		leader5UnblockDelay.SetToHeatState(state);

		leader7UnblockDelay.SetToHeatState(state);
	}
}