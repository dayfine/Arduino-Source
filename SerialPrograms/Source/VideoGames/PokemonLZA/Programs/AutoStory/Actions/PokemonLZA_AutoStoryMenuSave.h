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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  MENU_SAVE_ACTION: X, R on the main menu (its "R Salva": the save is no
//  menu entry), the save's confirmation, then B back to the overworld, by
//  the screen on each look, 250 ms apart. A screen is pressed only once it
//  has shown on two looks in a row since the last press:
//  - the overworld's party bar: X (again only 2 s after the last X), or,
//    once saved, done;
//  - a tutorial card (TUTORIAL_CARD_READING): A. The first X of a save
//    file opens the menu under a "Save the game" card (console B,
//    2026-11-16: 03.08b's save hung 2 minutes on it);
//  - the main menu (MAIN_MENU_READING): R (again only 2 s after the last
//    R), or B once the save is confirmed: the menu back counts as saved;
//  - a choice, or the arrow at the confirmation's Save
//    (MENU_SAVE_BUTTON_READING): A, which after R confirms the save; B
//    once saved;
//  - a text box: A before the save is confirmed, B after it (the "saved"
//    text), which counts as saved;
//  - anything else: nothing.
//  Each change of screen is logged ("[PLZA save_from_menu] screen 2:
//  tutorial card at 0.5s"). Fails as "menu_save_timeout" when the party
//  bar is not back after the save within 30 s, by the looks' times, and as
//  GAME_NOT_RUNNING on the Switch's HOME. The screen after R is unseen on
//  console B so far (2026-11-16).
Pokemon::Outcome save_from_menu(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
