/*  BDSP AutoStory - Launch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Launch_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Launch_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  BDSP's launch policy driving the shared launch on a scripted console,
//  from HOME through the title and the load to the overworld.
void add_tests_AutoStory_Launch(UnitTestDatabase& database);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
