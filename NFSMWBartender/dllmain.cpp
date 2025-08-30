#include <Windows.h>

#include "MemoryEditor.h"
#include "HeatParameters.h"

#include "StateObserver.h"

#include "DestructionStrings.h"
#include "RadioCallsigns.h"
#include "GroundSupport.h"
#include "GeneralSettings.h"

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
    {
        // Helicopter shadow fix
        MemoryEditor::Write<float>(0.f, {0x903660});

        // Disappearing helicopter mini-map icon fix
        MemoryEditor::WriteToAddressRange(0x90, 0x579EA2, 0x579EAB); 

        // Heat-level fixes (credit: ExOptsTeam)
        MemoryEditor::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
        MemoryEditor::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
    }
    
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