#pragma once

#include <vector>
#include <string>
#include <string_view>

#include "../../Common/Globals.hpp"
#include "../../Common/ConfigParser.hpp"
#include "../../Common/ModContainers.hpp"

#include "../../Utilities/MemoryTools.hpp"



namespace CopNotifications
{
	// Feature setup --------------------------------------------------------------------------------------------------------------------------------

	bool anyFeatureEnabled = false;

	// Logging
	constexpr Globals::LogLiteral logTag  = "[NTF]";
	constexpr Globals::LogLiteral logName = "CopNotifications";

	// Vehicle maps
	RELEASE_CONSTINIT VEHICLE_MAP(std::string, copTypeToNotificationText, {});

	RELEASE_CONSTINIT VEHICLE_MAP(binary, copTypeToNotificationIcon, "COPS_TAKENOUT_ICON"_bin);





	// Auxiliary functions --------------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] const char* __fastcall GetNotificationText(const vault copType)
	{
		return copTypeToNotificationText.GetReference(copType).c_str();
	}





	// Assembly detours -----------------------------------------------------------------------------------------------------------------------------

	// Sets the notification text for destroyed cop vehicles
	ASSEMBLY_DETOUR(NotificationText, /* begin = */ 0x595B0D, /* end = */ 0x595C41)
	{
		__asm
		{
			mov ecx, dword ptr [esp + 0x54]
			call GetNotificationText // ecx: copType
			cmp byte ptr [eax], '\0'

			EXIT_ASSEMBLY_DETOUR(NotificationText)
		}
	}



	// Sets the notification icon for destroyed cop vehicles
	ASSEMBLY_DETOUR(NotificationIcon, 0x595C93, 0x595CA0)
	{
		__asm
		{
			push dword ptr [esp + 0x60] // copType
			mov ecx, offset copTypeToNotificationIcon
			call ModContainers::VehicleMap<binary>::GetValue

			EXIT_ASSEMBLY_DETOUR(NotificationIcon)
		}
	}





	// Initialisation helpers -----------------------------------------------------------------------------------------------------------------------

	[[nodiscard]] bool ExtractNotificationTexts(const ConfigParser::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<std::string_view> stringOrNames;

		parser.ExtractVectors<std::string_view, std::string_view>("Vehicles:Notifications", copNames, {stringOrNames});

		constexpr auto StringOrNameToNotification = [](const std::string_view stringOrName) -> std::string_view
		{
			const auto GetBinaryString = AsFunction<const char* __fastcall (int, binary)>(0x56BB80);

			if (const char* const binaryString = GetBinaryString(0, Globals::GetBinaryHash(stringOrName)))
				return binaryString; // is valid notification-string name in game database

			return stringOrName;
		};

		return copTypeToNotificationText.Fill
		(
			ModContainers::FillSetup(copNames,      Globals::GetVaultHash,      Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(stringOrNames, StringOrNameToNotification, ModContainers::AlwaysValid())
		);
	}



	[[nodiscard]] bool ExtractNotificationIcons(const ConfigParser::Parser& parser)
	{
		std::vector<std::string_view> copNames;
		std::vector<std::string_view> iconLabels;

		parser.ExtractVectors<std::string_view, std::string_view>("Notifications:Icons", copNames, {iconLabels});

		constexpr auto IsValidGlobalTexture = [](const binary iconKey) -> bool
		{
			const auto GetTextureInfo = AsFunction<address __cdecl (binary, bool, bool)>(0x503400);
			return GetTextureInfo(iconKey, /* includeUnloadedTextures = */ false, /* returnDefaultIfNotFound = */ false);
		};

		return copTypeToNotificationIcon.Fill
		(
			ModContainers::FillSetup(copNames,    Globals::GetVaultHash,  Globals::DoesVehicleTypeExist),
			ModContainers::FillSetup(iconLabels,  Globals::GetBinaryHash, IsValidGlobalTexture)
		);
	}



	[[nodiscard]] bool InitialiseNotifications(const ConfigParser::Parser& parser)
	{
		const bool textMapExtracted = ExtractNotificationTexts(parser);
		const bool iconMapExtracted = ExtractNotificationIcons(parser);

		if (textMapExtracted) PATCH_ASSEMBLY_DETOUR(NotificationText);
		if (iconMapExtracted) PATCH_ASSEMBLY_DETOUR(NotificationIcon);

		return (textMapExtracted or iconMapExtracted);
	}





	// State interface ------------------------------------------------------------------------------------------------------------------------------

	bool Initialise(ConfigParser::Parser& parser)
	{
		if constexpr (Globals::loggingEnabled)
			Globals::LogConfig(logTag, logName);

		if (not parser.ParseFile(Globals::pathBasic, Globals::fileCosmetic)) return false;

		// Destruction notifications (and code modifications)
		if (not InitialiseNotifications(parser)) return false; // no valid notifications; disable feature

		// Status flag
		anyFeatureEnabled = true;

		return true;
	}
}