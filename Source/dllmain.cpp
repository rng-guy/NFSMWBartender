
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

#include "Headers\Common\Globals.hpp"
#include "Headers\Common\HeatParameters.hpp"

#include "Headers\Utilities\MemoryTools.hpp"

#include "Headers\Features\Basic\GameBreaker.hpp"
#include "Headers\Features\Basic\NitrousCharge.hpp"
#include "Headers\Features\Basic\RadioSpeech.hpp"
#include "Headers\Features\Basic\CopDetection.hpp"
#include "Headers\Features\Basic\GroundSupport.hpp"
#include "Headers\Features\Basic\GeneralSettings.hpp"
#include "Headers\Features\Basic\HelicopterVision.hpp"
#include "Headers\Features\Basic\InteractiveMusic.hpp"
#include "Headers\Features\Basic\CopNotifications.hpp"

#include "Headers\Features\Advanced\PursuitObserver.hpp"

#include "Headers\Features\StateObserver.hpp"





// Hooking functions --------------------------------------------------------------------------------------------------------------------------------

address InitialiseBartenderOriginal = 0x0;

static void __cdecl InitialiseBartender
(
	const size_t  numArgs,
	const address argArray
) {
	const auto OriginalFunction = AsFunction<void __cdecl (size_t, address)>(InitialiseBartenderOriginal);

	// Call original function first
	OriginalFunction(numArgs, argArray);

	#ifdef _DEBUG
	while (not IsDebuggerPresent()); // halt until debugger is attached
	#endif

	// Initialise log and config parser
	constexpr LogLiteral logTag     = "[MOD]";
	constexpr LogLiteral logSection = " SESSION";

	if constexpr (Globals::loggingEnabled)
	{
		Globals::logger.Open("BartenderLog.txt");

		Globals::LogFull(); // force newline to separate launches
		Globals::LogFull(logSection, logTag, "Bartender v4.00.00");

		// Check for other mods
		constexpr std::array fileNames =
		{
			"X360Stuff.asi",
			"NFSMWUnlimiter.asi",    
			"XNFSMusicPlayer.asi", 
			"NFSMWSpeedFixer.asi",
			"NFSMWExtraOptions.asi", 
			"NFSMWLimitAdjuster.asi", 
			"NFSMWOpenLimitAdjuster_gcp.asi",
			"NFSMostWanted.WidescreenFix.asi"
		};

		for (const char* const fileName : fileNames)
			if (MemoryTools::IsModuleLoaded(fileName)) Globals::LogPlain('+', fileName);
	}

	HeatParameters::Parser parser(/* fileCapactity = */ 6, /* sectionCapacityPerFile = */ 30, /* pairCapacityPerSection = */ 25);

	// Parse and initialise "Basic" feature set
	Globals::basicSetEnabled |= CopNotifications::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= RadioSpeech     ::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= CopDetection    ::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= HelicopterVision::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= InteractiveMusic::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= GeneralSettings ::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= GroundSuppport  ::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= NitrousCharge   ::InitialiseFeatures(parser);
	Globals::basicSetEnabled |= GameBreaker     ::InitialiseFeatures(parser);

	parser.ClearCachedFiles();

	if (Globals::basicSetEnabled)
	{
		// Apply feature-specific fixes
		RadioSpeech     ::ApplyFixes();
		CopDetection    ::ApplyFixes();
		HelicopterVision::ApplyFixes();
		GeneralSettings ::ApplyFixes();
		GroundSuppport  ::ApplyFixes();

		// Remove helicopter blob-shadow
		MemoryTools::Write<float>(0.f, {0x903660});

		// Prevent Heat-level resets (credit: ExOptsTeam)
		MemoryTools::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
		MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
	}

	// Parse and initialise "Advanced" feature set
	Globals::advancedSetEnabled = PursuitObserver::InitialiseFeatures(parser);

	// Apply Heat and state observer
	if (Globals::basicSetEnabled or Globals::advancedSetEnabled)
		StateObserver::InitialiseFeatures(parser);

	if constexpr (Globals::loggingEnabled)
	{
		Globals::LogFull(logSection, logTag, "Features");

		Globals::LogPlain("Basic    set", (Globals::basicSetEnabled)    ? "enabled" : "disabled");
		Globals::LogPlain("Advanced set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");
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

	InitialiseBartenderOriginal = MemoryTools::ReplaceCall(0x6665B4, InitialiseBartender); // InitializeEverything (0x665FC0)

	return TRUE;
}