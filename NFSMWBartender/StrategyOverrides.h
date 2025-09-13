#pragma once

#include "Globals.h"
#include "HashContainers.h"
#include "PursuitFeatures.h"
#include "HeatParameters.h"
#include "MemoryEditor.h"



namespace StrategyOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat levels
	HeatParameters::Pair<float> playerSpeedThresholds(15.f); // kph
	
	HeatParameters::Pair<bool>  heavy3UnblockEnableds (false);
	HeatParameters::Pair<float> minHeavy3UnblockDelays(1.f);   // seconds
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

	// Conversions
	float baseSpeedThreshold = playerSpeedThresholds.current / 3.6f; // metres / second
	float jerkSpeedThreshold = baseSpeedThreshold * .625f;





	// StrategyManager class ------------------------------------------------------------------------------------------------------------------------

	class StrategyManager : public PursuitFeatures::PursuitReaction
	{
		using IntervalTimer = PursuitFeatures::IntervalTimer;



	private:

		int* const numStrategyVehicles = (int*)(this->pursuit + 0x18C);

		const bool* const    isJerk         = (bool*)(this->pursuit + 0x238);
		const int* const     pursuitStatus  = (int*)(this->pursuit + 0x218);
		const address* const heavyStrategy  = (address*)(this->pursuit + 0x194);
		const address* const leaderStrategy = (address*)(this->pursuit + 0x198);

		bool watchingHeavyStrategy3 = false;

		HashContainers::AddressSet vehiclesOfCurrentStrategy;
		HashContainers::AddressSet vehiclesOfUnblockedHeavy3;

		IntervalTimer heavy3UnblockTimer  = IntervalTimer(heavy3UnblockEnableds,  minHeavy3UnblockDelays,  maxHeavy3UnblockDelays);
		IntervalTimer heavy4UnblockTimer  = IntervalTimer(heavy4UnblockEnableds,  minHeavy4UnblockDelays,  maxHeavy4UnblockDelays);
		IntervalTimer leader5UnblockTimer = IntervalTimer(leader5UnblockEnableds, minLeader5UnblockDelays, maxLeader5UnblockDelays);
		IntervalTimer leader7UnblockTimer = IntervalTimer(leader7UnblockEnableds, minLeader7UnblockDelays, maxLeader7UnblockDelays);

		IntervalTimer* unblockTimer = nullptr;

		inline static HashContainers::AddressMap<StrategyManager*> pursuitToManager;

		inline static const auto ClearSupportRequest = (void (__thiscall*)(address))0x42BCF0;


		void UpdateNumStrategyVehicles() const
		{
			*(this->numStrategyVehicles) = this->vehiclesOfCurrentStrategy.size();
		}


		void Reset()
		{
			if (not this->unblockTimer) return;

			if (this->watchingHeavyStrategy3)
			{
				this->watchingHeavyStrategy3 = false;
				this->vehiclesOfUnblockedHeavy3.merge(this->vehiclesOfCurrentStrategy);
			}
			else this->vehiclesOfCurrentStrategy.clear();

			this->unblockTimer = nullptr;
			this->UpdateNumStrategyVehicles();
		}


		void UpdateUnblockTimer(const IntervalTimer::Setting setting)
		{
			if (not this->unblockTimer) return;

			this->unblockTimer->Update(setting);

			if constexpr (Globals::loggingEnabled)
			{
				if (this->unblockTimer->IsEnabled())
					Globals::logger.Log(this->pursuit, "[SGY] Unblocking in", this->unblockTimer->TimeLeft());

				else
					Globals::logger.Log(this->pursuit, "[SGY] Strategy blocking fully");
			}
		}


		void WatchHeavyStrategy()
		{
			this->Reset();

			const address strategy = *(this->heavyStrategy);

			if (not strategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [SGY] Invalid HeavyStrategy pointer in", this->pursuit);

				return;
			}

			const int strategyID = *((int*)strategy);

			switch (strategyID)
			{
			case 3:
				this->watchingHeavyStrategy3 = true;
				this->unblockTimer           = &(this->heavy3UnblockTimer);
				break;

			case 4:
				this->unblockTimer = &(this->heavy4UnblockTimer);
				break;

			default:
				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SGY] Watching HeavyStrategy", strategyID);

			this->UpdateUnblockTimer(IntervalTimer::Setting::ALL);
		}


		void WatchLeaderStrategy()
		{
			this->Reset();

			const address strategy = *(this->leaderStrategy);

			if (not strategy)
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log("WARNING: [SGY] Invalid LeaderStrategy pointer in", this->pursuit);

				return;
			}

			const int strategyID = *((int*)strategy);

			switch (strategyID)
			{
			case 5:
				this->unblockTimer = &(this->leader5UnblockTimer);
				break;

			case 7:
				this->unblockTimer = &(this->leader7UnblockTimer);
				break;

			default:
				return;
			}

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SGY] Watching LeaderStrategy", strategyID);

			this->UpdateUnblockTimer(IntervalTimer::Setting::ALL);
		}


		void CheckUnblockTimer() const
		{
			if (not this->unblockTimer)               return;
			if (not this->unblockTimer->HasExpired()) return;

			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log(this->pursuit, "[SGY] Unblocking request");

			this->ClearSupportRequest(this->pursuit);
		}


		static float __fastcall GetSpeedOfTarget(const address pursuit)
		{
			__asm
			{
				mov eax, dword ptr [ecx + 0x74] // AIPursuit::GetTarget
				mov ecx, dword ptr [eax + 0x1C] // Target::GetPhysicsObject

				test ecx, ecx
				je zero // no valid physics object

				mov edx, dword ptr [ecx]
				call dword ptr [edx + 0x54] // PhysicsObject::GetRigidBody
				mov edx, dword ptr [eax]

				mov ecx, eax
				call dword ptr [edx + 0x30] // RigidBody::GetSpeedXY

				jmp conclusion

				zero:
				fldz

				conclusion:
				// Nothing!
			}
		}


		static void MakeVehiclesFlee(HashContainers::AddressSet& copVehicles)
		{
			for (const address copVehicle : copVehicles)
				PursuitFeatures::MakeVehicleFlee(copVehicle);

			copVehicles.clear();
		}


		bool IsSearchModeActive() const
		{
			return (*(this->pursuitStatus) == 2);
		}


		void CheckHeavyVehicleStatus()
		{
			if ((not this->watchingHeavyStrategy3) and (this->vehiclesOfUnblockedHeavy3.empty())) return;

			const float speedThreshold = (*(this->isJerk)) ? jerkSpeedThreshold : baseSpeedThreshold;

			if ((this->GetSpeedOfTarget(this->pursuit) < speedThreshold) or (this->IsSearchModeActive()))
			{
				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(this->pursuit, "[SGY] Making HeavyStrategy 3 vehicles flee");

				this->MakeVehiclesFlee(this->vehiclesOfUnblockedHeavy3);

				if (this->watchingHeavyStrategy3)
				{
					this->MakeVehiclesFlee(this->vehiclesOfCurrentStrategy);
					this->ClearSupportRequest(this->pursuit);
				}
			}
		}
		


	public:

		explicit StrategyManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit) 
		{
			this->pursuitToManager.insert({this->pursuit, this});
		}


		~StrategyManager() override
		{
			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnGameplay() override
		{
			this->CheckHeavyVehicleStatus();
			this->CheckUnblockTimer();
		}


		void UpdateOnHeatChange() override
		{
			this->UpdateUnblockTimer(IntervalTimer::Setting::LENGTH);
			this->CheckUnblockTimer();
		}


		void ReactToAddedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				[[fallthrough]];

			case CopLabel::LEADER:
				this->vehiclesOfCurrentStrategy.insert(copVehicle);
				this->UpdateNumStrategyVehicles();
			}
		}


		void ReactToRemovedVehicle
		(
			const address  copVehicle,
			const CopLabel copLabel
		)
			override
		{
			switch (copLabel)
			{
			case CopLabel::HEAVY:
				if (this->vehiclesOfUnblockedHeavy3.erase(copVehicle) > 0) break;
				[[fallthrough]];

			case CopLabel::LEADER:
				if (this->vehiclesOfCurrentStrategy.erase(copVehicle) > 0)
				{
					if (this->vehiclesOfCurrentStrategy.empty())
						this->ClearSupportRequest(this->pursuit);

					else 
						this->UpdateNumStrategyVehicles();
				}
			}
		}


		static void __fastcall NotifyOfHeavyStrategy(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
				foundManager->second->WatchHeavyStrategy();

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SGY] HeavyStrategy of unknown pursuit", pursuit);
		}


		static void __fastcall NotifyOfLeaderStrategy(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
				foundManager->second->WatchLeaderStrategy();

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [SGY] LeaderStrategy of unknown pursuit", pursuit);
		}


		static void __fastcall NotifyOfClearedRequest(const address pursuit)
		{
			const auto foundManager = StrategyManager::pursuitToManager.find(pursuit);

			if (foundManager != StrategyManager::pursuitToManager.end())
			{
				foundManager->second->Reset();

				if constexpr (Globals::loggingEnabled)
					Globals::logger.Log(pursuit, "[SGY] Active request cleared");
			}
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
			je reset

			jmp dword ptr goalResetSkip

			reset:
			push ecx
			mov ebx, esp
			push 0x0

			jmp dword ptr goalResetExit
		}
	}



	constexpr address collapseFleeEntrance = 0x4438A4;
	constexpr address collapseFleeExit     = 0x4438A9;

	__declspec(naked) void CollapseFlee()
	{
		__asm
		{
			mov edx, dword ptr [ecx + 0x48]
			cmp edx, 0x73B87374
			jne conclusion

			mov edx, dword ptr [ecx]
			call dword ptr [edx + 0xC]

			conclusion:
			jmp collapseFleeExit
		}
	}



	constexpr address clearRequestEntrance = 0x42B431;
	constexpr address clearRequestExit     = 0x42B436;

	__declspec(naked) void ClearRequest()
	{
		__asm
		{
			mov dword ptr [ebp + 0x78], edi

			push ecx
			lea ecx, dword ptr [ebp - 0x194]
			call StrategyManager::NotifyOfClearedRequest // ecx: pursuit
			pop ecx

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
			call StrategyManager::NotifyOfLeaderStrategy // ecx: pursuit

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
			call StrategyManager::NotifyOfHeavyStrategy // ecx: pursuit

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
			call StrategyManager::NotifyOfHeavyStrategy // ecx: pursuit

			// Execute original code and resume
			mov ecx, dword ptr [esi]
			mov dword ptr [esi + 0x78], 0x2

			jmp dword ptr heavyStrategy4Exit
		}
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		parser.LoadFile(HeatParameters::configPathAdvanced + "Strategies.ini");

		// Heat parameters
		HeatParameters::Parse<float>(parser, "Heavy3:Fleeing", {playerSpeedThresholds, 0.f});

		HeatParameters::ParseOptionalInterval
		(
			parser,
			"Heavy3:Unblocking",
			heavy3UnblockEnableds,
			minHeavy3UnblockDelays,
			maxHeavy3UnblockDelays
		);

		HeatParameters::ParseOptionalInterval
		(
			parser,
			"Heavy4:Unblocking",
			heavy4UnblockEnableds,
			minHeavy4UnblockDelays,
			maxHeavy4UnblockDelays
		);

		HeatParameters::ParseOptionalInterval
		(
			parser,
			"Leader5:Unblocking",
			leader5UnblockEnableds,
			minLeader5UnblockDelays,
			maxLeader5UnblockDelays
		);

		HeatParameters::ParseOptionalInterval
		(
			parser,
			"Leader7:Unblocking",
			leader7UnblockEnableds,
			minLeader7UnblockDelays,
			maxLeader7UnblockDelays
		);

		// Code caves
		MemoryEditor::Write<byte>   (0xE9,   {0x44384A}); // skip vanilla "CollapseSpeed" HeavyStrategy check
		MemoryEditor::Write<address>(0x02A3, {0x44384B});
		
		MemoryEditor::WriteToAddressRange(0x90, 0x4240BD, 0x4240C3); // OnAttached increment
		MemoryEditor::WriteToAddressRange(0x90, 0x42B71B, 0x42B72E); // OnDetached decrement

		MemoryEditor::DigCodeCave(GoalReset,      goalResetEntrance,      goalResetExit);
		MemoryEditor::DigCodeCave(ClearRequest,   clearRequestEntrance,   clearRequestExit);
		MemoryEditor::DigCodeCave(LeaderStrategy, leaderStrategyEntrance, leaderStrategyExit);
		MemoryEditor::DigCodeCave(HeavyStrategy3, heavyStrategy3Entrance, heavyStrategy3Exit);
		MemoryEditor::DigCodeCave(HeavyStrategy4, heavyStrategy4Entrance, heavyStrategy4Exit);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		playerSpeedThresholds.SetToHeat(isRacing, heatLevel);

		baseSpeedThreshold = playerSpeedThresholds.current / 3.6f;
		jerkSpeedThreshold = baseSpeedThreshold * .625f;

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
			Globals::logger.Log("    HEAT [SGY] StrategyOverrides");
			Globals::logger.LogLongIndent("playerSpeedThreshold    ", playerSpeedThresholds.current);

			if (heavy3UnblockEnableds.current)
				Globals::logger.LogLongIndent("heavy3UnblockDelay      ", minHeavy3UnblockDelays.current, "to", maxHeavy3UnblockDelays.current);

			if (heavy4UnblockEnableds.current)
				Globals::logger.LogLongIndent("heavy4UnblockDelay      ", minHeavy4UnblockDelays.current, "to", maxHeavy4UnblockDelays.current);

			if (leader5UnblockEnableds.current)
				Globals::logger.LogLongIndent("leader5UnblockDelay     ", minLeader5UnblockDelays.current, "to", maxLeader5UnblockDelays.current);

			if (leader7UnblockEnableds.current)
				Globals::logger.LogLongIndent("leader7UnblockDelay     ", minLeader7UnblockDelays.current, "to", maxLeader7UnblockDelays.current);
		}
	}
}