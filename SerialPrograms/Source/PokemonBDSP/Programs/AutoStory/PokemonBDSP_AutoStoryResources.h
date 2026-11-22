/*  BDSP AutoStory Resources
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The files under the Resources folder that AutoStory, and the readers
 *  and detectors it calls, load while a program runs. A deployment whose
 *  Packages lack one of them stops mid-story with a FileException, so the
 *  test checks every one of them exists.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryResources_H
#define PokemonAutomation_PokemonBDSP_AutoStoryResources_H

#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Paths relative to RESOURCE_PATH(). When a reader or detector that
//  AutoStory uses starts loading a new file, add it here.
const std::vector<std::string_view>& autostory_resource_paths();


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
