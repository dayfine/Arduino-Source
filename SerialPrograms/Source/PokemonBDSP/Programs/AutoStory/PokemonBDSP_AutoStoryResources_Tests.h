/*  BDSP AutoStory Resources Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryResources_Tests_H
#define PokemonAutomation_PokemonBDSP_AutoStoryResources_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Adds the test that every path in autostory_resource_paths() is under RESOURCE_PATH().
void add_tests_AutoStoryResources(UnitTestDatabase& database);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
