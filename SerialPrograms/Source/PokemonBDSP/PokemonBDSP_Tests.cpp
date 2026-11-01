/*  Pokemon BDSP Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Actions_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Maps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_RoutePlanner_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Segment00_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryMaps_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryWorld_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StorySimulation_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable_Tests.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Walk_Tests.h"
#include "PokemonBDSP_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



void add_tests(UnitTestDatabase& database){
    add_tests_AutoStory_Actions(database);
    add_tests_AutoStory_Maps(database);
    add_tests_AutoStory_StoryWorld(database);
    add_tests_AutoStory_RoutePlanner(database);
    add_tests_AutoStory_StoryMaps(database);
    add_tests_AutoStory_Segment00(database);
    add_tests_AutoStory_StorySimulation(database);
    add_tests_AutoStory_StoryTable(database);
    add_tests_AutoStory_Walk(database);
}



}
}
}
