#pragma once

#include <vector>
#include <string>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"



namespace CopNotifications
{

	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool featureEnabled = false;

	// Code caves 
	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<std::string> copTypeToNotificationText({});
	RELEASE_CONSTINIT ModContainers::DefaultVaultMap<binary>      copTypeToNotificationIcon("COPS_TAKENOUT_ICON"_bin);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	const char* __fastcall GetNotificationText(const vault copType)
	{
		return copTypeToNotificationText.GetReference(copType).c_str();
	}





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address notificationTextEntrance = 0x595B0D;
	constexpr address notificationTextExit     = 0x595C41;

	// Gets the notification text for destroyed cop vehicles
	__declspec(naked) void NotificationText()
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x54]
			call GetNotificationText // ecx: copType
			cmp byte ptr [eax], '\0'

			jmp dword ptr [notificationTextExit]
		}
	}



	constexpr address notificationIconEntrance = 0x595C93;
	constexpr address notificationIconExit     = 0x595CA0;

	// Gets the notification icon for destroyed cop vehicles
	__declspec(naked) void NotificationIcon()
	{
		__asm
		{
			push dword ptr [esp + 0x60] // copType
			mov ecx, offset copTypeToNotificationIcon
			call ModContainers::DefaultVaultMap<binary>::GetValue

			jmp dword ptr [notificationIconExit]
		}
	}





	// Parsing functions ----------------------------------------------------------------------------------------------------------------------------

	bool ParseNotificationTexts(const HeatParameters::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<std::string_view> stringOrNames;

		parser.ParseUser<std::string_view, std::string_view>("Vehicles:Notifications", copNames, {stringOrNames});

		const auto StringOrNameToNotification = [](const std::string_view stringOrName) -> std::string
		{
			const auto        GetBinaryString = AsFunction<const char* __fastcall (int, binary)>(0x56BB80);
			const char* const binaryString    = GetBinaryString(0, Globals::GetBinaryHash(stringOrName));

			return (binaryString) ? std::string(binaryString) : std::string(stringOrName);
		};

		return copTypeToNotificationText.FillFromVectors
		(
			"Vehicle-to-text",
			HeatParameters::configDefaultVaultHash,
			ModContainers::MapFillSetup(copNames,      Globals::GetVaultHash,      Globals::DoesVehicleTypeExist),
			ModContainers::MapFillSetup(stringOrNames, StringOrNameToNotification, ModContainers::AlwaysValid{})
		);
	}



	bool ParseNotificationIcons(const HeatParameters::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<std::string_view> iconLabels;

		parser.ParseUser<std::string_view, std::string_view>("Notifications:Icons", copNames, {iconLabels});

		const auto IsValidGlobalTexture = [](const binary iconKey) -> bool
		{
			const auto GetTextureInfo = AsFunction<address __cdecl (binary, bool, bool)>(0x503400);
			return GetTextureInfo(iconKey, /* includeUnloadedTextures = */ false, /* returnDefaultIfNotFound = */ false);
		};

		return copTypeToNotificationIcon.FillFromVectors
		(
			"Vehicle-to-icon",
			HeatParameters::configDefaultVaultHash,
			ModContainers::MapFillSetup(copNames,    Globals::GetVaultHash,  Globals::DoesVehicleTypeExist),
			ModContainers::MapFillSetup(iconLabels,  Globals::GetBinaryHash, IsValidGlobalTexture)
		);
	}



	bool ParseNotifications(const HeatParameters::Parser& parser)
	{
		const bool textMapIsValid = ParseNotificationTexts(parser);
		const bool iconMapIsValid = ParseNotificationIcons(parser);

		if (textMapIsValid) MemoryTools::MakeRangeJMP<notificationTextEntrance, notificationTextExit>(NotificationText);
		if (iconMapIsValid) MemoryTools::MakeRangeJMP<notificationIconEntrance, notificationIconExit>(NotificationIcon);

		return (textMapIsValid or iconMapIsValid);
	}





	// State management -----------------------------------------------------------------------------------------------------------------------------

	bool Initialise(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::logger.Log("  CONFIG [NTF] CopNotifications");

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Destruction notifications (and code modifications)
		if (not ParseNotifications(parser)) return false; // no valid notifications; disable feature

		// Status flag
		featureEnabled = true;

		return true;
	}
}