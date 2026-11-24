/*  AutoStory Resources Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks that every file in autostory_resource_paths() is in the
 *  Resources folder the program runs with.
 *
 */

#include "Pokemon/AutoStory/Pokemon_AutoStoryResources.h"
#include "PokemonFRLG_AutoStoryResources.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void add_tests_AutoStoryResources(UnitTestDatabase& database){
    Pokemon::add_tests_AutoStoryResourceList(
        database,
        "PokemonFRLG::AutoStoryResources",
        autostory_resource_paths
    );
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
