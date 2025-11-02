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





// DLL hook boilerplate -----------------------------------------------------------------------------------------------------------------------------

BOOL WINAPI DllMain
(
    HINSTANCE hinstDLL,
    DWORD     fdwReason,
    LPVOID    lpvReserved
) {
    if (fdwReason == DLL_PROCESS_ATTACH) 
        ApplyBartender();

    return TRUE;
}