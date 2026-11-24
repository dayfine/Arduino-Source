/*  AutoStory Resources
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared helper for verifying that every file under RESOURCE_PATH() a
 *  game's AutoStory program loads actually exists in the deployment's
 *  Resources folder.
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStoryResources_H
#define PokemonAutomation_Pokemon_AutoStoryResources_H

#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/FilePath.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Returns the entries of `paths` (each relative to `root`) that do not
//  exist under `root`, preserving input order.
std::vector<std::string> missing_autostory_resources(
    std::span<const std::string_view> paths,
    const Filesystem::Path& root
);

//  Registers a unit test named "<suite_name> - every file AutoStory loads is
//  in Resources" checking that every path returned by `paths()` exists under
//  RESOURCE_PATH().
void add_tests_AutoStoryResourceList(
    UnitTestDatabase& database,
    std::string_view suite_name,
    const std::vector<std::string_view>& (*paths)()
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
