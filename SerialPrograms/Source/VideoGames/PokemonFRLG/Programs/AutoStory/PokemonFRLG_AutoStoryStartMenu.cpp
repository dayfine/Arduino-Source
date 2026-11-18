/*  AutoStory Start Menu
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


int start_menu_rows(StartMenu menu){
    switch (menu){
    case StartMenu::BEFORE_POKEMON:
        return 5;
    case StartMenu::BEFORE_POKEDEX:
        return 6;
    case StartMenu::FULL:
    case StartMenu::SAFARI:
        return 7;
    }
    return 7;
}
int start_menu_save_row(StartMenu menu){
    return start_menu_rows(menu) - 3;
}
int start_menu_option_row(StartMenu menu){
    return start_menu_rows(menu) - 2;
}

StartMenuContext start_menu_context(StartMenu menu){
    switch (menu){
    case StartMenu::BEFORE_POKEDEX:
        return StartMenuContext::NO_DEX;
    case StartMenu::SAFARI:
        return StartMenuContext::SAFARI_ZONE;
    case StartMenu::BEFORE_POKEMON:
    case StartMenu::FULL:
        return StartMenuContext::STANDARD;
    }
    return StartMenuContext::STANDARD;
}

StartMenu step_start_menu(const Pokemon::StoryActionArgs& args){
    const bool* safari = game_arg<bool>(args, "safari");
    return safari != nullptr && *safari ? StartMenu::SAFARI : StartMenu::FULL;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
