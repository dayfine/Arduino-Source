/*  PokemonLZA AutoStory - Menu save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The run's own save, from the main menu, where a checkpoint would
 *  otherwise end on an autosave the plan does not place (a catch's, which
 *  lands wherever the catch was): the next checkpoint then starts from a
 *  fixed spot (the user, 2026-11-16).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMenuSave_H
#define PokemonAutomation_PokemonLZA_AutoStoryMenuSave_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  MENU_SAVE_ACTION: X, the main menu's Save (save_game_to_menu()), then B
//  back to the overworld's party bar. Fails as "menu_save_failed" when the
//  game is not saved, and as "menu_not_left" when the party bar does not
//  come back.
Pokemon::Outcome save_from_menu(StoryEnv& env, PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
