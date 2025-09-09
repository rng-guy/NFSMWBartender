#include <Windows.h>

#include "MemoryEditor.h"
#include "HeatParameters.h"

#include "StateObserver.h"

#include "DestructionStrings.h"
#include "RadioCallsigns.h"
#include "GroundSupport.h"
#include "GeneralSettings.h"
#include "GameAdjustments.h"

#include "PursuitObserver.h"





// Parsing and injection ----------------------------------------------------------------------------------------------------------------------------

static void Initialise() 
{
    HeatParameters::Parser parser;

    // "Basic" feature set
    Globals::basicSetEnabled |= DestructionStrings::Initialise(parser);
    Globals::basicSetEnabled |= RadioCallsigns    ::Initialise(parser);
    Globals::basicSetEnabled |= GroundSupport     ::Initialise(parser);
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

BOOL APIENTRY DllMain
(
    HMODULE hModule, 
    DWORD   ul_reason_for_call, 
    LPVOID  lpReserved
) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) Initialise();

    return TRUE;
}