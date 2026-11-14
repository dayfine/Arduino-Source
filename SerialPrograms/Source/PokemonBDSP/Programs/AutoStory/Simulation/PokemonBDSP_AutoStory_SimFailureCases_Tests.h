/*  BDSP AutoStory - Console failure cases on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SimFailureCases_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SimFailureCases_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{



//  Each failure console B showed in events and perception, run on the story
//  simulation; the ones the program still fails are known failing.
void add_tests_AutoStory_SimFailureCases(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
