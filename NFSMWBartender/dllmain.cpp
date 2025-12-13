#include <Windows.h>

#include "MemoryTools.h"
#include "HeatParameters.h"

#include "StateObserver.h"

#include "DestructionStrings.h"
#include "HelicopterVision.h"
#include "InteractiveMusic.h"
#include "RadioCallsigns.h"
#include "CopDetection.h"

#include "GeneralSettings.h"
#include "GroundSupports.h"

#include "PursuitObserver.h"





// Initialisation and injection ---------------------------------------------------------------------------------------------------------------------

static void ApplyBartender() 
{
    HeatParameters::Parser parser;

    // "Basic" feature set
    Globals::basicSetEnabled |= DestructionStrings::Initialise(parser);
    Globals::basicSetEnabled |= HelicopterVision  ::Initialise(parser);
    Globals::basicSetEnabled |= InteractiveMusic  ::Initialise(parser);
    Globals::basicSetEnabled |= RadioCallsigns    ::Initialise(parser);
    Globals::basicSetEnabled |= CopDetection      ::Initialise(parser);
  
    Globals::basicSetEnabled |= GeneralSettings::Initialise(parser);
    Globals::basicSetEnabled |= GroundSupports ::Initialise(parser);

    if (Globals::basicSetEnabled)
    {
        // Feature-linked fixes
        if (not CopDetection::featureEnabled)
            CopDetection::ApplyFixes();

        if (not GroundSupports::featureEnabled)
            GroundSupports::ApplyFixes();

        if (not GeneralSettings::featureEnabled)
            GeneralSettings::ApplyFixes();

        // Helicopter shadow
        MemoryTools::Write<float>(0.f, {0x903660});

        // Heat-level reset (credit: ExOptsTeam)
        MemoryTools::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
        MemoryTools::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
    }
    
    // "Advanced" feature set
    Globals::advancedSetEnabled = PursuitObserver::Initialise(parser);

    // General Heat and state observer
    if (Globals::basicSetEnabled or Globals::advancedSetEnabled)
        StateObserver::Initialise(parser);
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
        else ApplyBartender();
    }   

    return TRUE;
}