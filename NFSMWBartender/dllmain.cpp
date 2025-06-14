#include <Windows.h>

#include "ConfigParser.h"
#include "StateObserver.h"
#include "DestructionStrings.h"
#include "Miscellaneous.h"
#include "GroundSupport.h"
#include "PursuitBar.h"

#include "PursuitObserver.h"



// Parsing and injection ----------------------------------------------------------------------------------------------------------------------------

static void Initialise() 
{
    ConfigParser::Parser parser;

    GroundSupport::Initialise(parser);
    DestructionStrings::Initialise(parser);
    Miscellaneous::Initialise(parser);
    PursuitBar::Initialise(parser);
    PursuitObserver::Initialise(parser);

    StateObserver::Initialise();
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
