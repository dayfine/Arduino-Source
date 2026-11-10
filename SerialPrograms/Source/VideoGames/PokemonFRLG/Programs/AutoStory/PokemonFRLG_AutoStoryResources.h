/*  AutoStory Resources
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The files under the Resources folder that AutoStory, and the readers
 *  and detectors it calls, load while a program runs. A deployment whose
 *  Packages lack one of them stops mid-story with a FileException, so the
 *  test checks every one of them exists.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryResources_H
#define PokemonAutomation_PokemonFRLG_AutoStoryResources_H

#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Paths relative to RESOURCE_PATH(). When a reader or detector that
//  AutoStory uses starts loading a new file, add it here.
const std::vector<std::string_view>& autostory_resource_paths();

//  Adds the test that every path above is under RESOURCE_PATH().
void add_tests_AutoStoryResources(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
