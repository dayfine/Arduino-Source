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

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  MENU_SAVE_ACTION: X, R on the main menu (the save is no menu entry),
//  confirm, then B back to the overworld; each screen is pressed only once
//  it shows on two looks in a row. The first X of a save file opens the
//  menu under a tutorial card, which gets A. Fails as "menu_save_timeout"
//  when not back on the overworld within 30 s, and as GAME_NOT_RUNNING on
//  the Switch's HOME.
Pokemon::Outcome save_from_menu(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
