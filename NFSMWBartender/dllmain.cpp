#include <Windows.h>

#include "MemoryTools.h"
#include "HeatParameters.h"

#include "StateObserver.h"

#include "DestructionStrings.h"
#include "InteractiveMusic.h"
#include "RadioCallsigns.h"
#include "GroundSupports.h"
#include "GeneralSettings.h"
#include "GameAdjustments.h"

#include "PursuitObserver.h"





// Initialisation and injection ---------------------------------------------------------------------------------------------------------------------

static void ApplyBartender() 
{
    HeatParameters::Parser parser;

    // "Basic" feature set
    Globals::basicSetEnabled |= DestructionStrings::Initialise(parser);
    Globals::basicSetEnabled |= InteractiveMusic  ::Initialise(parser);
    Globals::basicSetEnabled |= RadioCallsigns    ::Initialise(parser);
    Globals::basicSetEnabled |= GroundSupports    ::Initialise(parser);
    Globals::basicSetEnabled |= GeneralSettings   ::Initialise(parser);

    if (Globals::basicSetEnabled)
        GameAdjustments::Initialise(parser);
    
    // "Advanced" feature set
    Globals::advancedSetEnabled = PursuitObserver::Initialise(parser);

    // General Heat and state observer
    if (Globals::basicSetEnabled or Globals::advancedSetEnabled)
        StateObserver::Initialise(parser);
}



static bool IsExecutableCompatible()
{
    // Credit: thelink2012 and MWisBest
    const auto base = (uintptr_t)(GetModuleHandleA(NULL));
    const auto dos  = (PIMAGE_DOS_HEADER)base;
    const auto nt   = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);

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
            MessageBoxA(NULL, "This .exe is not compatible with Bartender.\nSee Bartender's README for help.", "NFSMW Bartender", MB_ICONERROR);

            return FALSE;
        }
        else ApplyBartender();
    }   

    return TRUE;
}