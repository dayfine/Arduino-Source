/*  BDSP AutoStory Walk Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Walk_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Walk_Tests_H

#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  The walker's screen reads: indoors() on synthetic screens and
//  screenshots, the door's fade, and the location banner.
void add_tests_AutoStory_Walk(UnitTestDatabase& database);



}
}
}
#endif
