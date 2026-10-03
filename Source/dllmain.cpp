
// Compatibility checks -----------------------------------------------------------------------------------------------------------------------------

#ifndef _MSC_VER
#error "Bartender requires MSVC."

#elif (_MSC_VER < 1930)
#error "Bartender requires Visual Studio 2022 or newer."

#elif ((not defined(_WIN32)) or defined(_WIN64))
#error "Bartender requires 32-bit Windows."

#elif ((not defined(_MSVC_LANG)) or (_MSVC_LANG < 202002L))
#error "Bartender requires C++20 or newer."

#endif





// Project includes ---------------------------------------------------------------------------------------------------------------------------------

#include <array>

#include <Windows.h>

#ifdef _DEBUG
#include <debugapi.h>
#endif

#include "Headers/Common/Globals.hpp"
#include "Headers/Common/ConfigParser.hpp"

#include "Headers/Utilities/MemoryTools.hpp"

#include "Headers/Features/FeatureSets.hpp"
#include "Headers/Features/Basic/BasicFeatures.hpp"
#include "Headers/Features/Advanced/AdvancedFeatures.hpp"





// Hook functions -----------------------------------------------------------------------------------------------------------------------------------

HOOK_ORIGINAL(Initialise);

static void __cdecl Initialise
(
	const size_t  numArgs, 
	const address argArray
) {
	CALL_HOOK_ORIGINAL(Initialise, numArgs, argArray);

	#ifdef _DEBUG
	while (not IsDebuggerPresent()); // halt until debugger is attached
	#endif

	constexpr Globals::LogLiteral logTag     = "[MOD]";
	constexpr Globals::LogLiteral logSection = " SESSION";

	if constexpr (Globals::loggingEnabled)
	{
		Globals::logger.Open("BartenderLog.txt");

		Globals::LogFull(); // force newline to separate sessions
		Globals::LogFull(logSection, logTag, "Bartender v4.00.00");

		constexpr std::array modNames =
		{
			"X360Stuff.asi",
			"Mempoolulator.asi",
			"NFSMWUnlimiter.asi",    
			"XNFSMusicPlayer.asi", 
			"NFSMWSpeedFixer.asi",
			"NFSMWAntagoNISt.asi",
			"NFSMWExtraOptions.asi",
			"NFSMWHDReflections.asi",
			"NFSMWLimitAdjuster.asi",
			"NFSMWDaylightSavingTime.asi",
			"NFSMWOpenLimitAdjuster_gcp.asi",
			"NFSMostWanted.WidescreenFix.asi"
		};

		for (const char* const modName : modNames)
		{
			if (MemoryTools::IsModuleLoaded(modName))
				Globals::LogPlain('+', modName);
		}
	}

	ConfigParser::Parser parser
	(
		/* fileCapactity          = */ 6, 
		/* sectionCapacityPerFile = */ 30, 
		/* pairCapacityPerSection = */ 25
	);

	FeatureSets::Initialise(parser);

	if constexpr (Globals::loggingEnabled)
	{
		Globals::LogFull(logSection, logTag, "Features");

		Globals::LogPlain("Basic    set", (BasicFeatures   ::anyFeatureEnabled) ? "enabled" : "disabled");
		Globals::LogPlain("Advanced set", (AdvancedFeatures::anyFeatureEnabled) ? "enabled" : "disabled");
	}
}





// DLL hook boilerplate -----------------------------------------------------------------------------------------------------------------------------

BOOL WINAPI DllMain
(
	const HINSTANCE hinstDLL,
	const DWORD     fdwReason,
	const LPVOID    lpvReserved
) {
	if (fdwReason != DLL_PROCESS_ATTACH) return TRUE;

	if (MemoryTools::GetEntryPoint() != 0x3C4040) // .exe-dependent entry point
	{
		MessageBoxA(NULL, "This .exe isn't compatible with Bartender.\nSee Bartender's README for help.", "NFSMW Bartender", MB_ICONERROR);

		return FALSE; // should never happen (assuming the user has actually read the README, which... yeah...)
	}

	PATCH_HOOK_FUNCTION(Initialise, 0x6665B4); // InitializeEverything (0x665FC0)

	return TRUE;
}