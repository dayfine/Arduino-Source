/*  Pokemon BDSP Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Inference/PokemonBDSP_DialogDetector.h"
#include "Inference/PokemonBDSP_MenuDetector.h"
#include "Programs/AutoStory/PokemonBDSP_AutoStory_Maps_Tests.h"
#include "Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WildBattlePolicy_Tests.h"
#include "PokemonBDSP_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



void add_tests(UnitTestDatabase& database){
    add_tests_DialogDetector(database);
    add_tests_MenuDetector(database);
    add_tests_AutoStory_Maps(database);
    add_tests_AutoStory_WildBattlePolicy(database);
}



}
}
}
