#pragma once

#include <vector>
#include <fstream>
#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace CopDetection
{
	// ColourTracker class --------------------------------------------------------------------------------------------------------------------------

	class ColourTracker
	{
	private: // aliases

		using TimeQuery = float ();


	private: // members

		TimeQuery* GetTimestamp;

		float lastUpdateTimestamp = 0.f; // seconds
		bool  forceNextUpdate     = true;
		

	public: // methods

		consteval explicit ColourTracker(TimeQuery* const GetTimestamp) : GetTimestamp(GetTimestamp) {}


		[[nodiscard]] bool YieldShouldUpdate()
		{
			const float timestamp = this->GetTimestamp();

			constexpr float updateInterval = 1.f / 30.f; // seconds

			if ((not this->forceNextUpdate) and (timestamp < this->lastUpdateTimestamp + updateInterval))
			{
				// Guard against potential wrap-around / reset
				if (timestamp >= this->lastUpdateTimestamp) return false;
			}

			this->lastUpdateTimestamp = timestamp;
			this->forceNextUpdate     = false;

			return true;
		}


		void Reset()
		{
			this->forceNextUpdate = true;
		}
	};





	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[DET]";
	constexpr Globals::LogLiteral logName = "CopDetection";

	// Types
	struct Detection
	{
	// Members

		float radarRange;       // metres
		float patrolIconRange;  // metres
		float pursuitIconRange; // metres

		bool keepsIcon;
	};

	// Inline hashes for ASM
	enum class VaultHash : vault
	{
		CHOPPER = "CHOPPER"_vlt
	};

	// Vehicle maps
	RELEASE_CONSTINIT VEHICLE_MAP(Detection, copTypeToDetection, {300.f, 0.f, 300.f, true});

	// Assembly detours
	constinit ColourTracker miniMapCops (Globals::GetGameplayTime);
	constinit ColourTracker worldMapCops(Globals::GetNonGameplayTime);

	bool updateWorldMapColours = false;





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool __fastcall ShouldDrawIcon
	(
		const address copVehicle,
		const address playerVehicle
	) {
		const address copAIVehicle = Globals::GetAIVehicleOfVehicle(copVehicle);
		ASSERT_CONDITION_THEN_IF_FALSE(copAIVehicle, return false);

		bool& iconIsKept = AsReference<bool>(copAIVehicle - 0x4C + 0x81); // padding byte
		if (iconIsKept) return true; // mini-map icon already kept

		// Update whether the vehicle's been in a pursuit before
		bool& hasBeenInPursuit = AsReference<bool>(copAIVehicle - 0x4C + 0x82); // padding byte

		if (not hasBeenInPursuit)
		{
			const bool hasPursuit  = AsReference<address>(copAIVehicle + 0x70);
			const bool isRoadblock = AsReference<address>(copAIVehicle + 0x74);

			hasBeenInPursuit = (hasPursuit or isRoadblock);
		}

		// Fetch icon-range data for vehicle type
		const auto& detection = copTypeToDetection.GetReference(Globals::GetVehicleType(copVehicle));
		const float iconRange = (hasBeenInPursuit) ? detection.pursuitIconRange : detection.patrolIconRange;

		if (iconRange <= 0.f) return false;

		// Check distance to player vehicle
		const auto GetVehiclePosition = AsFunction<address __thiscall (address)>(0x688340);

		const address copPosition    = GetVehiclePosition(copVehicle);
		const address playerPosition = GetVehiclePosition(playerVehicle);

		const auto GetSquaredDistance = AsFunction<float __cdecl (address, address)>(0x401930);
		if (GetSquaredDistance(copPosition, playerPosition) > iconRange * iconRange) return false;
		
		iconIsKept = detection.keepsIcon;

		return true;
	}



	[[nodiscard]] float __fastcall GetRadarRange(const address copVehicle)
	{
		const vault copType = Globals::GetVehicleType(copVehicle);
		return copTypeToDetection.GetReference(copType).radarRange;
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Makes cop icons on the world map flash independently of frame rate
	ASSEMBLY_DETOUR(WorldMapUpdate, /* begin = */ 0x55B770, /* end = */ 0x55B776)
	{
		__asm
		{
			mov ecx, offset worldMapCops
			call ColourTracker::YieldShouldUpdate
			mov byte ptr [updateWorldMapColours], al

			// Execute original code and resume
			mov edi, dword ptr [esi + 0x154]

			EXIT_ASSEMBLY_DETOUR(WorldMapUpdate)
		}
	}



	// Decides which cop vehicle gets a mini-map icon
	ASSEMBLY_DETOUR(CopVehicleIcon, 0x579EDF, 0x579EE5)
	{
		static constexpr address skipExit = 0x57A09F;

		using enum VaultHash;

		__asm
		{
			// Execute original code first
			cmp eax, CHOPPER
			je conclusion // is helicopter

			cmp byte ptr [anyFeatureEnabled], 1
			jne limitation // map feature disabled

			mov ecx, dword ptr [esi]
			mov edx, dword ptr [ebp + 0x8]
			call ShouldDrawIcon // ecx: copVehicle; edx: playerVehicle
			test al, al
			je skip             // do not draw icon

			limitation:
			cmp dword ptr [esp + 0x18], 8 // icon count
			jge skip                      // at icon cap

			conclusion:
			EXIT_ASSEMBLY_DETOUR(CopVehicleIcon)

			skip:
			jmp dword ptr [skipExit]
		}
	}



	// Sets the cop radar's detection range for cop vehicles
	ASSEMBLY_DETOUR(CopVehicleRadar, 0x6EE206, 0x6EE20C)
	{
		__asm
		{
			mov ecx, esi
			call GetRadarRange // ecx: copVehicle
			fxch st(1)
			fcompp

			EXIT_ASSEMBLY_DETOUR(CopVehicleRadar)
		}
	}



	// Ensures destroyed roadblock vehicles get white mini-map icons
	ASSEMBLY_DETOUR(DestructionCheck, 0x57A034, 0x57A03D)
	{
		__asm
		{
			// Execute original code first
			cmp byte ptr [edi + 0x8], 0
			je conclusion // not in pursuit

			mov ecx, dword ptr [esi]
			call Globals::IsVehicleDestroyed
			cmp al, 1

			conclusion:
			EXIT_ASSEMBLY_DETOUR(DestructionCheck)
		}
	}



	// Makes cop icons on the mini-map flash at a consistent pace
	ASSEMBLY_DETOUR(MiniMapCopColours, 0x579E16, 0x579E1C)
	{
		__asm
		{
			push eax

			mov ecx, offset miniMapCops
			call ColourTracker::YieldShouldUpdate
			test al, al

			pop ecx
			je conclusion // not yet time

			inc ecx

			conclusion:
			cmp ecx, 8 + 1 // colour ticks

			EXIT_ASSEMBLY_DETOUR(MiniMapCopColours)
		}
	}



	// Makes cop icons on the world map flash at a consistent pace
	ASSEMBLY_DETOUR(WorldMapCopColours, 0x51F70C, 0x51F712)
	{
		__asm
		{
			mov ecx, dword ptr [esi + 0x38]

			cmp byte ptr [updateWorldMapColours], 1
			jne conclusion // not yet time

			inc ecx

			conclusion:
			mov eax, ecx

			EXIT_ASSEMBLY_DETOUR(WorldMapCopColours)
		}
	}



	// Prepares the cop-icon state when a new mini-map is created
	ASSEMBLY_DETOUR(MiniMapConstructor, 0x59DA9B, 0x59DAA0)
	{
		__asm
		{
			mov ecx, offset miniMapCops
			call ColourTracker::Reset

			// Execute original code and resume
			mov ecx, dword ptr [esp + 0x1C]
			pop edi

			EXIT_ASSEMBLY_DETOUR(MiniMapConstructor)
		}
	}



	// Prepares the cop-icon state when a new world map is created
	ASSEMBLY_DETOUR(WorldMapConstructor, 0x5614FF, 0x561505)
	{
		__asm
		{
			push eax

			mov ecx, offset worldMapCops
			call ColourTracker::Reset

			pop eax
			
			// Execute original code and resume
			mov dword ptr [esi + 0x124], eax

			EXIT_ASSEMBLY_DETOUR(WorldMapConstructor)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	std::fstream& operator<<
	(
		std::fstream&    stream,
		const Detection& detection
	) {
		stream << detection.radarRange;

		constexpr std::string_view delimiter = ", ";

		stream << delimiter << detection.patrolIconRange;
		stream << delimiter << detection.pursuitIconRange;

		stream << delimiter << ((detection.keepsIcon) ? "true" : "false");

		return stream;
	}



	[[nodiscard]] bool ExtractDetections(const ConfigParser::Parser& parser)
	{
		std::vector<std::string_view> copNames;

		std::vector<float> radarRanges;
		std::vector<float> patrolIconRanges;
		std::vector<float> pursuitIconRanges;
		std::vector<bool>  keepsIcons;

		const size_t numCopVehicles = parser.ExtractVectors<std::string_view, float, float, float, bool>
		(
			"Vehicles:Detection", copNames, {radarRanges, {0.f}}, {patrolIconRanges, {0.f}}, {pursuitIconRanges, {0.f}}, {keepsIcons}
		);

		std::vector<Detection> detections(numCopVehicles);

		for (size_t vehicleID = 0; vehicleID < numCopVehicles; ++vehicleID)
		{
			detections[vehicleID] =
			{
				.radarRange       = radarRanges      [vehicleID],
				.patrolIconRange  = patrolIconRanges [vehicleID],
				.pursuitIconRange = pursuitIconRanges[vehicleID],
				.keepsIcon        = keepsIcons       [vehicleID]
			};
		}

		return copTypeToDetection.Fill
		(
			ModContainers::FillSetup(copNames,   Globals::GetVaultHash,         Globals::IsVehicleTypeCar),
			ModContainers::FillSetup(detections, ModContainers::IdentityCopy(), ModContainers::AlwaysValid())
		);
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	void ApplyFixes()
	{
		// Disappearing helicopter icon (and some features)
		MemoryTools::MakeRangeNOP<0x579EA2, 0x579EAB>(); // early icon-counter check

		PATCH_ASSEMBLY_DETOUR(CopVehicleIcon);
		PATCH_ASSEMBLY_DETOUR(DestructionCheck);

		// Update frequency for cop-icon colours
		PATCH_ASSEMBLY_DETOUR(WorldMapUpdate);
		PATCH_ASSEMBLY_DETOUR(MiniMapCopColours);
		PATCH_ASSEMBLY_DETOUR(WorldMapCopColours);
		PATCH_ASSEMBLY_DETOUR(MiniMapConstructor);
		PATCH_ASSEMBLY_DETOUR(WorldMapConstructor);
	}



	bool InitialiseFeatures(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileCosmetic)) return false;

		// Radar detection
		if (not ExtractDetections(parser)) return false; // no valid settings; disable feature

		// Code modifications
		MemoryTools::MakeRangeNOP<0x579E33, 0x579E69>(); // pursuit check
		MemoryTools::MakeRangeNOP<0x579EE5, 0x579EEA>(); // non-pursuit icon flag
		MemoryTools::MakeRangeNOP<0x579EF0, 0x579F0E>(); // engagement-radius check
		MemoryTools::MakeRangeNOP<0x579FCD, 0x579FFD>(); // icon-flag checks

		PATCH_ASSEMBLY_DETOUR(CopVehicleRadar);

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}