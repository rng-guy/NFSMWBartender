#include <Windows.h>

#include "MemoryEditor.h"
#include "HeatParameters.h"

#include "StateObserver.h"

#include "DestructionStrings.h"
#include "GroundSupport.h"
#include "Miscellaneous.h"
#include "PursuitBar.h"

#include "PursuitObserver.h"



// Parsing and injection ----------------------------------------------------------------------------------------------------------------------------

static void Initialise() 
{
    HeatParameters::Parser parser;

    // "Basic" feature set
    bool basicSetEnabled = false;

    basicSetEnabled |= DestructionStrings::Initialise(parser);
    basicSetEnabled |= GroundSupport     ::Initialise(parser);
    basicSetEnabled |= Miscellaneous     ::Initialise(parser);
    basicSetEnabled |= PursuitBar        ::Initialise(parser);

    if (basicSetEnabled) 
    {
        // Helicopter shadow fix
        MemoryEditor::Write<float>(0.f, {0x903660});

        // Heat-level fixes (credit: ExOptsTeam)
        MemoryEditor::Write<float>       (HeatParameters::maxHeat,    {0x7BB502, 0x7B1387, 0x7B0C89, 0x7B4D7C, 0x435088});
        MemoryEditor::Write<const float*>(&(HeatParameters::maxHeat), {0x435079, 0x7A5B03, 0x7A5B12});
    }

    // "Advanced" feature set
    const bool advancedSetEnabled = PursuitObserver::Initialise(parser);

    // General Heat and state observer
    if (basicSetEnabled or advancedSetEnabled) 
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