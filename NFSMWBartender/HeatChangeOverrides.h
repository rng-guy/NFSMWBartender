#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"

#include "PursuitFeatures.h"



namespace HeatChangeOverrides
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Heat parameters
	constinit HeatParameters::Pair<bool> heatTimerEneableds(true);

	constinit HeatParameters::Pair<float> chaserHeatChanges    (0.f);
	constinit HeatParameters::Pair<float> supportHeatChanges   (0.f);
	constinit HeatParameters::Pair<float> helicopterHeatChanges(0.f);

	constinit HeatParameters::Pair<float> roadblockHeatChanges(0.f);
	constinit HeatParameters::Pair<float> spikesHeatChanges   (0.f);

	constinit HeatParameters::Pair<float> copWreckHeatChanges(0.f);

	constinit HeatParameters::Pair<float> copHitHeatChanges    (0.f);
	constinit HeatParameters::Pair<float> trafficHitHeatChanges(0.f);

	constinit HeatParameters::Pair<float> heatChangePerAssaults(0.f);
	constinit HeatParameters::Pair<bool>  onlyOneAssaultPerCops(true);

	constinit HeatParameters::Pair<float> damageHeatChanges(0.f);

	// Code caves
	size_t lastAnimatedHeatLevel = 0;
	float  animationEndTimestamp = 0.f;

	constinit ModContainers::DefaultCopyVaultMap<float> copTypeToHeatChange(0.f);





	// HeatManager class ----------------------------------------------------------------------------------------------------------------------------

	class HeatManager : public PursuitFeatures::PursuitReaction
	{
	private:

		// Internal CostTracker class for event-based Heat changes
		class CountTracker
		{
		private:

			int lastCount = 0;

			const volatile int& count;

			const HeatParameters::Pair<float>& heatPerCounts;



		public:

			explicit CountTracker
			(
				const address                      pursuit,
				const int                          offset,
				const HeatParameters::Pair<float>& heatPerCounts
			)
				: count(*reinterpret_cast<volatile int*>(pursuit + offset)), heatPerCounts(heatPerCounts)
			{
			}


			explicit CountTracker(const CountTracker&)   = delete;
			CountTracker& operator=(const CountTracker&) = delete;

			explicit CountTracker(CountTracker&&)   = delete;
			CountTracker& operator=(CountTracker&&) = delete;


			float UpdateCount()
			{
				const int change = this->count - this->lastCount;

				this->lastCount += change;
				if (change <= 0) return 0.f;

				return Globals::floatScale * static_cast<float>(change) * this->heatPerCounts.current;
			}
		};



	private:

		float pendingHeatChange = 0.f;

		std::array<CountTracker, 9> trackers =
		{
			CountTracker{this->pursuit, 0x184, chaserHeatChanges},
			CountTracker{this->pursuit, 0x188, supportHeatChanges},
			CountTracker{this->pursuit, 0x150, helicopterHeatChanges},

			CountTracker{this->pursuit, 0x158, roadblockHeatChanges},
			CountTracker{this->pursuit, 0x17C, spikesHeatChanges},

			CountTracker{this->pursuit, 0x13C, copWreckHeatChanges},

			CountTracker{this->pursuit, 0x15C, copHitHeatChanges},
			CountTracker{this->pursuit, 0x168, trafficHitHeatChanges},

			CountTracker{this->pursuit, 0x174, damageHeatChanges}
		};
	
		inline static constinit ModContainers::AddressMap<HeatManager*> pursuitToManager;


		void UpdateTrackers()
		{
			if (not Globals::playerHeatLevelKnown) return;

			float totalHeatChange = 0.f;

			for (auto& tracker : this->trackers)
				totalHeatChange += tracker.UpdateCount();

			if (not Globals::IsInCooldownMode(this->pursuit))
				this->pendingHeatChange += totalHeatChange;
		}


		static HeatManager* FindManager(const address pursuit)
		{
			const auto foundManager = HeatManager::pursuitToManager.find(pursuit);

			if (foundManager != HeatManager::pursuitToManager.end())
				return foundManager->second;

			else if constexpr (Globals::loggingEnabled)
				Globals::logger.Log("WARNING: [CNG] No manager for pursuit", pursuit);

			return nullptr;
		}



	public:

		explicit HeatManager(const address pursuit) : PursuitFeatures::PursuitReaction(pursuit)
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('+', this, "HeatManager");

			this->pursuitToManager.try_emplace(this->pursuit, this);
		}


		~HeatManager() override
		{
			if constexpr (Globals::loggingEnabled)
				Globals::logger.Log<2>('-', this, "HeatManager");

			this->pursuitToManager.erase(this->pursuit);
		}


		void UpdateOnGameplay() override
		{
			this->UpdateTrackers();
		}


		static void __stdcall AddToPendingHeatChange
		(
			const address pursuit,
			const float   amount
		) {
			if (Globals::IsInCooldownMode(pursuit)) return;

			HeatManager* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return; // should never happen

			manager->pendingHeatChange += Globals::floatScale * amount;
		}


		static float __fastcall GetPendingHeatChange(const address pursuit)
		{
			if (Globals::IsInCooldownMode(pursuit)) return 0.f;

			HeatManager* const manager = HeatManager::FindManager(pursuit);
			if (not manager) return 0.f; // should never happen

			const float heatChange = manager->pendingHeatChange;

			if constexpr (Globals::loggingEnabled)
			{
				if (heatChange != 0.f)
					Globals::logger.Log(pursuit, "[CNG] Heat change:", heatChange);
			}

			manager->pendingHeatChange = 0.f;

			return heatChange;
		}
	};





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	float ClampHeat(const float heat)
	{
		return std::clamp<float>(heat, 1.f, HeatParameters::maxHeat);
	}



	void __fastcall ClampHeatLimits(const address pursuit)
	{
		volatile float& minHeat = *reinterpret_cast<volatile float*>(pursuit + 0x9C);
		volatile float& maxHeat = *reinterpret_cast<volatile float*>(pursuit + 0xA0);

		minHeat = ClampHeat(minHeat);
		maxHeat = ClampHeat(maxHeat);

		if (minHeat > maxHeat)
			minHeat = maxHeat;
	}



	bool __stdcall ProcessCollision
	(
		const address pursuit,
		const address copVehicle,
		const bool    racerAtFault
	) {
		const address copAIVehiclePursuit = Globals::GetAIVehiclePursuit(copVehicle);
		if (not copAIVehiclePursuit) return false; // should never happen

		volatile bool& damagedByRacer = *reinterpret_cast<volatile bool*>(copAIVehiclePursuit + 0xB);

		if (not damagedByRacer)
		{
			damagedByRacer = true;

			if (pursuit)
			{
				const auto NotifyCopDamaged = reinterpret_cast<void (__thiscall*)(address, address)>(0x40AF40);

				NotifyCopDamaged(pursuit, copVehicle);
			}		
		}

		if (not racerAtFault) return false;

		volatile bool& assaultedByRacer = *reinterpret_cast<volatile bool*>(copAIVehiclePursuit - 0x758 + 0x76A); // padding byte

		if (pursuit and (not (onlyOneAssaultPerCops.current and assaultedByRacer)))
			HeatManager::AddToPendingHeatChange(pursuit, heatChangePerAssaults.current);

		assaultedByRacer = true;

		return (pursuit and Globals::IsPlayerPursuit(pursuit));
	}



	void __fastcall UpdateHeatAnimation(const address heatMeter)
	{
		constexpr bool useUnpausedTime = false;

		const float  gameTime         = Globals::GetGameTime(useUnpausedTime);
		const size_t currentHeatLevel = static_cast<size_t>(*reinterpret_cast<float*>(heatMeter + 0x40));
		
		if (gameTime >= animationEndTimestamp)
		{
			const auto IsScriptSet = reinterpret_cast<bool (__cdecl*)(address, uint32_t)>      (0x514DA0);
			const auto SetScript   = reinterpret_cast<void (__cdecl*)(address, uint32_t, bool)>(0x514D10);

			const address  interfaceObject = *reinterpret_cast<address*>(heatMeter + 0x44);
			const uint32_t animationScript = (currentHeatLevel != lastAnimatedHeatLevel) ? 0x41E1FEDC : 0x1744B3;

			if (not IsScriptSet(interfaceObject, animationScript))
			{
				SetScript(interfaceObject, animationScript, true);

				if (animationScript == 0x41E1FEDC)
					animationEndTimestamp = gameTime + 2.5f; // animation length (seconds)
			}
		}

		lastAnimatedHeatLevel = currentHeatLevel;
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address heatLimitsEntrance = 0x443171;
	constexpr address heatLimitsExit     = 0x44317A;

	// Ensures the Heat limits of pursuits are valid
	__declspec(naked) void HeatLimits()
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x48]
			call ClampHeatLimits // ecx: pursuit

			// Execute original code and resume
			push ebx
			mov ecx, esi
			mov dword ptr [esi + 0xE0], ebx
			
			jmp dword ptr heatLimitsExit
		}
	}



	constexpr address passiveHeatEntrance = 0x443D4A;
	constexpr address passiveHeatExit     = 0x443D50;

	// Adds pending Heat changes from pursuit observers to racer Heat updates
	__declspec(naked) void PassiveHeat()
	{
		__asm
		{
			lea ecx, dword ptr [esi + 0x40]
			call HeatManager::GetPendingHeatChange // ecx: pursuit
			faddp st(1), st(0)
			fstp dword ptr [esp + 0x1C]            // new perp Heat

			test ebx, ebx
			je conclusion // no pursuit attributes 

			cmp byte ptr heatTimerEneableds.current, 0x0

			conclusion:
			jmp dword ptr passiveHeatExit
		}
	}



	constexpr address spikeCounterEntrance = 0x43E654;
	constexpr address spikeCounterExit     = 0x43E663;

	// Increments the "spikes deployed" counter correctly
	__declspec(naked) void SpikeCounter()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x10]
			cmp dword ptr [ecx], 0x3 // prop ID
			jne conclusion           // prop not spike strip

			mov edx, dword ptr [esp + 0x4C4] // roadblock pursuit
			inc dword ptr [edx + 0x17C]      // spike strips deployed

			conclusion:
			jmp dword ptr spikeCounterExit
		}
	}



	constexpr address supportCheckEntrance = 0x423FA2;
	constexpr address supportCheckExit     = 0x423FF1;

	// Checks for supports to increment deployment counter
	__declspec(naked) void SupportCheck()
	{
		static constexpr address IsSupportVehicle = 0x419890;

		__asm
		{
			// Execute original code first
			mov ebx, eax
			add esp, 0x4

			push edi // copVehicle
			lea ecx, dword ptr [esi + 0x10]
			call dword ptr IsSupportVehicle
			cmp al, 0x1

			jmp dword ptr supportCheckExit
		}
	}



	constexpr address perpCollisionEntrance = 0x429C8B;
	constexpr address perpCollisionExit     = 0x429CBB;

	// Checks whether collisions with cops constitute assault
	__declspec(naked) void PerpCollision()
	{
		__asm
		{
			movzx eax, byte ptr [esp + 0x13]
			mov ebx, dword ptr [esp + 0x14]
			
			push eax // racerAtFault
			push edi // copVehicle
			push ebx // pursuit
			call ProcessCollision
			test al, al

			jmp dword ptr perpCollisionExit
		}
	}



	constexpr address heatMeterResetEntrance = 0x59CEDF;
	constexpr address heatMeterResetExit     = 0x59CEE5;

	// Prepares the meter state when a new Heat meter is created
	__declspec(naked) void HeatMeterReset()
	{
		__asm
		{
			// Execute original code first
			push edi
			xor ebx, ebx
			lea edi, dword ptr [esi + 0x8]

			mov dword ptr lastAnimatedHeatLevel, ebx
			mov dword ptr animationEndTimestamp, ebx

			jmp dword ptr heatMeterResetExit
		}
	}



	constexpr address heatMeterUpdateEntrance = 0x56676D;
	constexpr address heatMeterUpdateExit     = 0x5667C8;

	// Manages the Heat-transition animation of the Heat meter
	__declspec(naked) void HeatMeterUpdate()
	{
		__asm
		{
			mov ecx, esi
			call UpdateHeatAnimation // ecx: heatMeter

			jmp dword ptr heatMeterUpdateExit
		}
	}



	constexpr address typeDestructionEntrance = 0x418F99;
	constexpr address typeDestructionExit     = 0x418F9F;

	// Notifies managers of destroyed cop vehicles for Heat-change purposes
	__declspec(naked) void TypeDestruction()
	{
		__asm
		{
			// Execute original code first
			mov dword ptr [esi + 0xF8], eax // last cop type destroyed

			push eax // copType
			mov ecx, offset copTypeToHeatChange
			call ModContainers::DefaultCopyVaultMap<float>::GetValue

			push eax
			fstp dword ptr [esp] // amount
			push esi             // pursuit
			call HeatManager::AddToPendingHeatChange

			jmp dword ptr typeDestructionExit
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	void ParseDamageChanges(const HeatParameters::Parser& parser)
	{
		HeatParameters::Pair<int> damageToHeats(0);

		HeatParameters::Parse(parser, "Heat:Damage", damageToHeats);

		for (const bool forRaces : {false, true})
		{
			const auto& damages = damageToHeats    .GetValues(forRaces);
			auto&       changes = damageHeatChanges.GetValues(forRaces);

			for (const size_t heatLevel : HeatParameters::heatLevels)
			{
				const int damage = damages[heatLevel - 1];

				if (damage != 0)
					changes[heatLevel - 1] = 1.f / static_cast<float>(damage);
			}
		}
	}



	bool ParseVehicleChanges(const HeatParameters::Parser& parser)
	{
		std::vector<const char*> copVehicles; // for game compatibility
		std::vector<float>       heatChanges;

		parser.ParseUser<const char*, float>("Wrecking:Vehicles", copVehicles, {heatChanges});

		return copTypeToHeatChange.FillFromVectors
		(
			"Vehicle-to-change",
			Globals::GetVaultKey(HeatParameters::configDefaultKey),
			ModContainers::FillSetup(copVehicles, Globals::GetVaultKey, Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(heatChanges)
		);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [CNG] HeatChangeOverrides");

		parser.LoadFile(HeatParameters::configPathAdvanced, "Heat.ini");

		// Heat parameters
		HeatParameters::Parse(parser, "Heat:Time",       heatTimerEneableds);
		HeatParameters::Parse(parser, "Heat:Deployment", chaserHeatChanges,    supportHeatChanges, helicopterHeatChanges);
		HeatParameters::Parse(parser, "Heat:Roadblocks", roadblockHeatChanges, spikesHeatChanges);

		HeatParameters::Parse(parser, "Heat:Wrecking", copWreckHeatChanges);

		HeatParameters::Parse(parser, "Heat:Collisions",    copHitHeatChanges,     trafficHitHeatChanges);
		HeatParameters::Parse(parser, "Collisions:Assault", heatChangePerAssaults, onlyOneAssaultPerCops);

		ParseDamageChanges(parser);

		// Vehicle-specific Heat changes
		if (ParseVehicleChanges(parser))
		{
			// Code modifications (feature-specific)
			MemoryTools::MakeRangeJMP<typeDestructionEntrance, typeDestructionExit>(TypeDestruction);
		}

		// Code modifications (general)
		MemoryTools::Write<byte>(0xEB, {0x44307F}); // Heat limits in Challenge Series events

		MemoryTools::MakeRangeNOP<0x429C74, 0x429C7F>(); // first perp-damage check

		MemoryTools::MakeRangeJMP<heatLimitsEntrance,      heatLimitsExit>     (HeatLimits);
		MemoryTools::MakeRangeJMP<passiveHeatEntrance,     passiveHeatExit>    (PassiveHeat);
		MemoryTools::MakeRangeJMP<spikeCounterEntrance,    spikeCounterExit>   (SpikeCounter);
		MemoryTools::MakeRangeJMP<supportCheckEntrance,    supportCheckExit>   (SupportCheck);
		MemoryTools::MakeRangeJMP<perpCollisionEntrance,   perpCollisionExit>  (PerpCollision);
		MemoryTools::MakeRangeJMP<heatMeterResetEntrance,  heatMeterResetExit> (HeatMeterReset);
		MemoryTools::MakeRangeJMP<heatMeterUpdateEntrance, heatMeterUpdateExit>(HeatMeterUpdate);

		// Status flag
		featureEnabled = true;

		return true;
	}



	void LogHeatReport()
	{
		Globals::logger.Log("    HEAT [CNG] HeatChangeOverrides");

		heatTimerEneableds.Log("heatTimerEneabled       ");

		chaserHeatChanges    .Log("chaserHeatChange        ");
		supportHeatChanges   .Log("supportHeatChange       ");
		helicopterHeatChanges.Log("helicopterHeatChange    ");

		roadblockHeatChanges.Log("roadblockHeatChange     ");
		spikesHeatChanges   .Log("spikesHeatChange        ");

		copWreckHeatChanges.Log("copWreckHeatChange      ");

		copHitHeatChanges    .Log("copHitHeatChange        ");
		trafficHitHeatChanges.Log("trafficHitHeatChange    ");
		heatChangePerAssaults.Log("heatChangePerAssault    ");
		onlyOneAssaultPerCops.Log("onlyOneAssaultPerCop    ");

		damageHeatChanges.Log("damageHeatChange        ");
	}



	void SetToHeat
	(
		const bool   isRacing,
		const size_t heatLevel
	) {
		if (not featureEnabled) return;

		heatTimerEneableds.SetToHeat(isRacing, heatLevel);

		chaserHeatChanges    .SetToHeat(isRacing, heatLevel);
		supportHeatChanges   .SetToHeat(isRacing, heatLevel);
		helicopterHeatChanges.SetToHeat(isRacing, heatLevel);

		roadblockHeatChanges.SetToHeat(isRacing, heatLevel);
		spikesHeatChanges   .SetToHeat(isRacing, heatLevel);

		copWreckHeatChanges.SetToHeat(isRacing, heatLevel);

		copHitHeatChanges    .SetToHeat(isRacing, heatLevel);
		trafficHitHeatChanges.SetToHeat(isRacing, heatLevel);

		heatChangePerAssaults.SetToHeat(isRacing, heatLevel);
		onlyOneAssaultPerCops.SetToHeat(isRacing, heatLevel);

		damageHeatChanges.SetToHeat(isRacing, heatLevel);

		if constexpr (Globals::loggingEnabled)
			LogHeatReport();
	}
}