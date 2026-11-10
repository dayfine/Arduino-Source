/*  BDSP AutoStory Leg Trace Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LegTrace_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LegTrace_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The simulation's leg records of a row, against a golden copy beside this
//  file, and the same on every run.
void add_tests_AutoStory_LegTrace(UnitTestDatabase& database);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
