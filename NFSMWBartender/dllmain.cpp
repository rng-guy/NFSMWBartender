
// Compatibility checks -----------------------------------------------------------------------------------------------------------------------------

#if !defined(_MSC_VER)
#error "Bartender requires MSVC."

#elif (_MSC_VER < 1930)
#error "Bartender requires Visual Studio 2022 or newer."

#elif !defined(_WIN32) || defined(_WIN64)
#error "Bartender requires 32-bit Windows."

#elif !defined(_MSVC_LANG) || (_MSVC_LANG < 202002L)
#error "Bartender requires C++20 or newer."

#endif





// Project includes ---------------------------------------------------------------------------------------------------------------------------------

#include <Windows.h>

#ifdef _DEBUG
	#include <debugapi.h>
#endif

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "GameBreaker.h"
#include "RadioChatter.h"
#include "CopDetection.h"
#include "GroundSupport.h"
#include "GeneralSettings.h"
#include "HelicopterVision.h"
#include "InteractiveMusic.h"
#include "DestructionStrings.h"

#include "PursuitObserver.h"

#include "StateObserver.h"





// Initialisation and injection ---------------------------------------------------------------------------------------------------------------------

address InitialiseBartenderOriginal = 0x0;

static void __cdecl InitialiseBartender
(
	const size_t  numArgs,
	const address argArray
) {
	const auto OriginalFunction = reinterpret_cast<void (__cdecl*)(size_t, address)>(InitialiseBartenderOriginal);

	// Call original function first
	OriginalFunction(numArgs, argArray);

	// Halt until debugger is attached
	#ifdef _DEBUG
		while (not IsDebuggerPresent());
	#endif

	// Initialise log and config parser
	if constexpr (Globals::loggingEnabled)
	{
		Globals::logger.Open("BartenderLog.txt");
		Globals::logger.Log ("\n SESSION [MOD] Bartender v3.05.00");

		if (MemoryTools::IsModuleLoaded("NFSMWUnlimiter.asi"))             Globals::logger.Log<2>("+ Unlimiter");
		if (MemoryTools::IsModuleLoaded("NFSMWExtraOptions.asi"))          Globals::logger.Log<2>("+ ExtraOptions");
		if (MemoryTools::IsModuleLoaded("NFSMWLimitAdjuster.asi"))         Globals::logger.Log<2>("+ LimitAdjuster");
		if (MemoryTools::IsModuleLoaded("NFSMWOpenLimitAdjuster_gcp.asi")) Globals::logger.Log<2>("+ OpenLimitAdjuster");
	}

	constexpr size_t configFileCapacity     = 6;  // files
	constexpr size_t sectionCapacityPerFile = 30; // sections
	constexpr size_t pairCapacityPerSection = 25; // pairs

	HeatParameters::Parser parser(configFileCapacity, sectionCapacityPerFile, pairCapacityPerSection);

	// Parse and initialise "Basic" feature set
	Globals::basicSetEnabled |= DestructionStrings::Initialise(parser);
	Globals::basicSetEnabled |= RadioChatter      ::Initialise(parser);
	Globals::basicSetEnabled |= CopDetection      ::Initialise(parser);
	Globals::basicSetEnabled |= HelicopterVision  ::Initialise(parser);
	Globals::basicSetEnabled |= InteractiveMusic  ::Initialise(parser);
	Globals::basicSetEnabled |= GeneralSettings   ::Initialise(parser);
	Globals::basicSetEnabled |= GroundSuppport    ::Initialise(parser);
	Globals::basicSetEnabled |= GameBreaker       ::Initialise(parser);

	parser.ClearCachedPaths();

	if (Globals::basicSetEnabled)
	{
		// Apply feature-specific fixes
		if (not RadioChatter    ::featureEnabled) RadioChatter    ::ApplyFixes();
		if (not CopDetection    ::featureEnabled) CopDetection    ::ApplyFixes();
		if (not HelicopterVision::featureEnabled) HelicopterVision::ApplyFixes();
		if (not GeneralSettings ::featureEnabled) GeneralSettings ::ApplyFixes();
		if (not GroundSuppport  ::featureEnabled) GroundSuppport  ::ApplyFixes();

		// Remove static helicopter shadow
		MemoryTools::Write<float>(0.f, {0x903660});

		// Prevent Heat-level resets (credit: ExOptsTeam)
		MemoryTools::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
		MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
	}

	// Parse and initialise "Advanced" feature set
	Globals::advancedSetEnabled = PursuitObserver::Initialise(parser);

	// Apply Heat and state observer
	if constexpr (Globals::loggingEnabled)
	{
		Globals::logger.Log(" SESSION [MOD] Features");

		Globals::logger.Log<2>("Basic    set", (Globals::basicSetEnabled)    ? "enabled" : "disabled");
		Globals::logger.Log<2>("Advanced set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");
	}

	if (Globals::basicSetEnabled or Globals::advancedSetEnabled)
		StateObserver::Initialise(parser);
}





// DLL hook boilerplate -----------------------------------------------------------------------------------------------------------------------------

BOOL WINAPI DllMain
(
	const HINSTANCE hinstDLL,
	const DWORD     fdwReason,
	const LPVOID    lpvReserved
) {
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (MemoryTools::GetEntryPoint() != 0x3C4040)
		{
			MessageBoxA(NULL, "This .exe isn't compatible with Bartender.\nSee Bartender's README for help.", "NFSMW Bartender", MB_ICONERROR);
			return FALSE; // should never happen (assuming the user has actually read the README, which... yeah...)
		}

		InitialiseBartenderOriginal = MemoryTools::MakeCallHook(0x6665B4, InitialiseBartender); // InitializeEverything (0x665FC0)
	}   

	return TRUE;
}