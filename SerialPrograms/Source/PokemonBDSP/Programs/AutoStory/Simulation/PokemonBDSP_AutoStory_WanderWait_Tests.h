/*  BDSP AutoStory Wander Wait Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WanderWait_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WanderWait_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The wait for the Mart clown on column 56 (00.05b, 00.05c), on the
//  simulation with him wandering his box.
void add_tests_AutoStory_WanderWait(UnitTestDatabase& database);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
