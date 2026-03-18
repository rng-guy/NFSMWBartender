#include <Windows.h>

#include "Globals.h"
#include "MemoryTools.h"
#include "HeatParameters.h"

#include "GameBreaker.h"
#include "RadioChatter.h"
#include "CopDetection.h"
#include "GroundSupports.h"
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

    // Apply hooked logic last
    if constexpr (Globals::loggingEnabled)
    {
        Globals::logger.Open("BartenderLog.txt");

        Globals::logger.Log("\n SESSION [VER] Bartender v3.02.00");
    }

    constexpr size_t maxNumConfigFiles = 6;
    HeatParameters::Parser parser(maxNumConfigFiles);

    Globals::basicSetEnabled |= DestructionStrings::Initialise(parser);
    Globals::basicSetEnabled |= RadioChatter      ::Initialise(parser);
    Globals::basicSetEnabled |= CopDetection      ::Initialise(parser);
    Globals::basicSetEnabled |= HelicopterVision  ::Initialise(parser);
    Globals::basicSetEnabled |= InteractiveMusic  ::Initialise(parser);
    Globals::basicSetEnabled |= GeneralSettings   ::Initialise(parser);
    Globals::basicSetEnabled |= GroundSupports    ::Initialise(parser);
    Globals::basicSetEnabled |= GameBreaker       ::Initialise(parser);

    parser.ClearCachedPaths();

    if (Globals::basicSetEnabled)
    {
        // Feature-linked fixes
        if (not RadioChatter::featureEnabled)
            RadioChatter::ApplyFixes();

        if (not CopDetection::featureEnabled)
            CopDetection::ApplyFixes();

        if (not HelicopterVision::featureEnabled)
            HelicopterVision::ApplyFixes();

        if (not GeneralSettings::featureEnabled)
            GeneralSettings::ApplyFixes();

        if (not GroundSupports::featureEnabled)
            GroundSupports::ApplyFixes();

        // Remove static helicopter shadow
        MemoryTools::Write<float>(0.f, {0x903660});

        // Prevent Heat-level resets (credit: ExOptsTeam)
        MemoryTools::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
        MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
    }

    // "Advanced" feature set
    Globals::advancedSetEnabled = PursuitObserver::Initialise(parser);

    // General Heat and state observer
    if constexpr (Globals::loggingEnabled)
    {
        Globals::logger.Log(" FEATURE [INI] Basic    feature set", (Globals::basicSetEnabled)    ? "enabled" : "disabled");
        Globals::logger.Log(" FEATURE [INI] Advanced feature set", (Globals::advancedSetEnabled) ? "enabled" : "disabled");
    }

    if (Globals::basicSetEnabled or Globals::advancedSetEnabled)
        StateObserver::Initialise(parser);
    
    else if constexpr (Globals::loggingEnabled)
        Globals::logger.Log(" SESSION [INI] Bartender disabled");
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

            return FALSE;
        }

        InitialiseBartenderOriginal = MemoryTools::MakeCallHook(0x6665B4, InitialiseBartender); // InitializeEverything (0x665FC0)
    }   

    return TRUE;
}