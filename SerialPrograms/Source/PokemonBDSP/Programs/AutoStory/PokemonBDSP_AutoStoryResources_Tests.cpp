/*  BDSP AutoStory Resources Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks that every file in autostory_resource_paths() is in the
 *  Resources folder the program runs with.
 *
 */

#include "Pokemon/AutoStory/Pokemon_AutoStoryResources.h"
#include "PokemonBDSP_AutoStoryResources.h"
#include "PokemonBDSP_AutoStoryResources_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


void add_tests_AutoStoryResources(UnitTestDatabase& database){
    Pokemon::add_tests_AutoStoryResourceList(
        database,
        "PokemonBDSP::AutoStoryResources",
        autostory_resource_paths
    );
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
