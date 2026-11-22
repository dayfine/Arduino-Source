/*  Pokemon BDSP Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Inference/Rng/PokemonBDSP_BlinkExtraction_Tests.h"
#include "Programs/RngManipulation/PokemonBDSP_BlinkRecovery_Tests.h"
#include "PokemonBDSP_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



void add_tests(UnitTestDatabase& database){
    add_tests_BlinkExtraction(database);
    add_tests_BlinkRecovery(database);
}



}
}
}
