/*  PokemonLZA AutoStory - Menu save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstddef>
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLines.h"
#include "PokemonLZA_AutoStoryMenuSave.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  B presses at most out of the main menu after the save.
constexpr size_t MENU_SAVE_B_PRESSES = 6;

} // namespace


Pokemon::Outcome save_from_menu(StoryEnv& env, PlzaBackend& backend){
    if (!save_game_to_menu(env.env.console, env.context)){
        return Pokemon::outcome_failed("menu_save_failed", "the main menu's Save did not save the game");
    }
    if (!press_b_out_of_menu(backend, MENU_SAVE_B_PRESSES)){
        return Pokemon::outcome_failed("menu_not_left", "no party bar after the save's B presses");
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
