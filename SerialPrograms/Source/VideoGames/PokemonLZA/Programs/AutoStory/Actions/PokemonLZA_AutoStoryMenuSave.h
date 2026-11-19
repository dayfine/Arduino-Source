/*  PokemonLZA AutoStory - Menu save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The run's own save, from the main menu, so the next checkpoint starts
 *  from a fixed spot rather than from wherever an autosave landed.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMenuSave_H
#define PokemonAutomation_PokemonLZA_AutoStoryMenuSave_H

#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PlzaAction::SAVE_FROM_MENU: X, R on the main menu (the save is no menu entry),
//  confirm, then B back to the overworld; each screen is pressed only once
//  it shows on two looks in a row. The first X of a save file opens the
//  menu under a tutorial card, which gets A. Fails as "menu_save_timeout"
//  when not back on the overworld within 30 s, and as GAME_NOT_RUNNING on
//  the Switch's HOME.
std::optional<Pokemon::StoryFailure> save_from_menu(PlzaBackend& backend);

//  PlzaAction::SET_TEXT_SPEED_FAST: X, 2 s for the menu, A on the first
//  menu's tutorial card while it shows (1.5 s apart), then blind presses to
//  the options' text speed and Fast (L, Up, A, Down, Right), then B until
//  the party bar or the controls hint shows on two looks in a row. Fails as

//  "text_speed_timeout" when not back within 30 s, and as GAME_NOT_RUNNING
//  on the Switch's HOME.
std::optional<Pokemon::StoryFailure> set_text_speed_fast(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
