#pragma once

#include <vector>
#include <string>
#include <string_view>

#include "Globals.h"
#include "MemoryTools.h"
#include "ModContainers.h"
#include "HeatParameters.h"
#include "PersistentStrings.h"



namespace CopNotifications
{
	// Parameters -----------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr LogLiteral logTag  = "[NTF]";
	constexpr LogLiteral logName = "CopNotifications";

	// Code caves 
	RELEASE_CONSTINIT DEFAULT_VAULT_MAP(const char*, copTypeToNotificationText, ""); // C-style for game compatibility
	RELEASE_CONSTINIT DEFAULT_VAULT_MAP(binary,      copTypeToNotificationIcon, "COPS_TAKENOUT_ICON"_bin);





	// Code caves -----------------------------------------------------------------------------------------------------------------------------------

	constexpr address notificationTextEntrance = 0x595B0D;
	constexpr address notificationTextExit     = 0x595C41;

	// Gets the notification text for destroyed cop vehicles
	__declspec(naked) void NotificationText()
	{
		__asm
		{
			push dword ptr [esp + 0x54] // copType
			mov ecx, offset copTypeToNotificationText
			call ModContainers::DefaultVaultMap<const char*>::GetValue
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

		const auto StringOrNameToNotification = [](const std::string_view stringOrName) -> const char*
		{
			const auto        GetBinaryString = AsFunction<const char* __fastcall (int, binary)>(0x56BB80);
			const char* const binaryString    = GetBinaryString(0, Globals::GetBinaryHash(stringOrName));

			return PersistentStrings::Create((binaryString) ? binaryString : stringOrName).c_str();
		};

		return copTypeToNotificationText.Fill
		(
			HeatParameters::configDefaultKey,
			ModContainers::FillSetup(copNames,      Globals::GetVaultHash,      Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(stringOrNames, StringOrNameToNotification, ModContainers::AlwaysValid())
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

		return copTypeToNotificationIcon.Fill
		(
			HeatParameters::configDefaultKey,
			ModContainers::FillSetup(copNames,    Globals::GetVaultHash,  Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(iconLabels,  Globals::GetBinaryHash, IsValidGlobalTexture)
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

	bool InitialiseFeatures(HeatParameters::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.LoadFile(HeatParameters::configPathBasic, "Cosmetic.ini")) return false;

		// Destruction notifications (and code modifications)
		if (not ParseNotifications(parser)) return false; // no valid notifications; disable feature

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}