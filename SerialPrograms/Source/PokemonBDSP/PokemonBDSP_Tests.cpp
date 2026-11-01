/*  Pokemon BDSP Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_MapGrid_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Maps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Segment00_Tests.h"
#include "PokemonBDSP_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



void add_tests(UnitTestDatabase& database){
    add_tests_AutoStory_Maps(database);
    add_tests_AutoStory_MapGrids(database);
    add_tests_AutoStory_Segment00(database);
}



}
}
}
