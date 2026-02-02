#include <Windows.h>

#include "MemoryTools.h"
#include "HeatParameters.h"

#include "StateObserver.h"

#include "DestructionStrings.h"
#include "HelicopterVision.h"
#include "InteractiveMusic.h"
#include "CopDetection.h"
#include "RadioChatter.h"

#include "GeneralSettings.h"
#include "GroundSupports.h"

#include "PursuitObserver.h"





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

        Globals::logger.Log("\n SESSION [VER] Bartender v3.00.01");
    }

    HeatParameters::Parser parser;

    parser.formatDefaultKey = HeatParameters::defaultValueHandle;

    Globals::basicSetEnabled |= DestructionStrings::Initialise(parser);
    Globals::basicSetEnabled |= HelicopterVision  ::Initialise(parser);
    Globals::basicSetEnabled |= InteractiveMusic  ::Initialise(parser);
    Globals::basicSetEnabled |= CopDetection      ::Initialise(parser);
    Globals::basicSetEnabled |= RadioChatter      ::Initialise(parser);

    Globals::basicSetEnabled |= GeneralSettings::Initialise(parser);
    Globals::basicSetEnabled |= GroundSupports ::Initialise(parser);

    if (Globals::basicSetEnabled)
    {
        // Feature-linked fixes
        if (not HelicopterVision::featureEnabled)
            HelicopterVision::ApplyFixes();

        if (not CopDetection::featureEnabled)
            CopDetection::ApplyFixes();

        if (not RadioChatter::featureEnabled)
            RadioChatter::ApplyFixes();

        if (not GeneralSettings::featureEnabled)
            GeneralSettings::ApplyFixes();

        if (not GroundSupports::featureEnabled)
            GroundSupports::ApplyFixes();

        // Remove static helicopter shadow
        MemoryTools::Write<float>(0.f, {0x903660});

        // Correct Heat limits in Challenge Series races
        MemoryTools::Write<byte>(0xEB, {0x44307F});

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
    {
        StateObserver::Initialise(parser);

        if constexpr (Globals::loggingEnabled)
        {
            if (MemoryTools::numRangeErrors > 0)
                Globals::logger.Log("WARNING: [INI]", static_cast<int>(MemoryTools::numRangeErrors), "RANGE ERRORS!");

            if (MemoryTools::numCaveErrors > 0)
                Globals::logger.Log("WARNING: [INI]", static_cast<int>(MemoryTools::numCaveErrors), "CAVE ERRORS!");

            if (MemoryTools::numHookErrors > 0)
                Globals::logger.Log("WARNING: [INI]", static_cast<int>(MemoryTools::numHookErrors), "HOOK ERRORS!");
        }
    }
    else if constexpr (Globals::loggingEnabled)
        Globals::logger.Log(" SESSION [INI] Bartender disabled");
}



static bool IsExecutableCompatible()
{
    // Credit: thelink2012 and MWisBest
    const auto base = reinterpret_cast<uintptr_t>        (GetModuleHandleA(NULL));
    const auto dos  = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    const auto nt   = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);

    return (nt->OptionalHeader.AddressOfEntryPoint == 0x3C4040);
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
        if (not IsExecutableCompatible())
        {
            MessageBoxA(NULL, "This .exe isn't compatible with Bartender.\nSee Bartender's README for help.", "NFSMW Bartender", MB_ICONERROR);

            return FALSE;
        }

        InitialiseBartenderOriginal = MemoryTools::MakeCallHook(InitialiseBartender, 0x6665B4); // InitializeEverything (0x665FC0)
    }   

    return TRUE;
}