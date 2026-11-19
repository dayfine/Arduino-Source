/*  PokemonLZA AutoStory - Story game on the console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's story game with the console's actions and reload.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleGame_H

#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The console's tools: its actions, the minimap arrow, lock-on and
//  battle code. `env` and `sightings` must outlive them.
//  `starter_turn`: the stick heading from 01.03's start to the starter picked.
PlzaStoryTools plza_console_tools(StoryEnv& env, AutosaveSightings& sightings, double starter_turn);

//  plza_story_game() with the console's reload of the latest save, which
//  says the game is at the failed checkpoint's start. `env` and
//  `sightings` must outlive the game.
Pokemon::StoryGame make_plza_story_game(StoryEnv& env, const StoryRows& rows, AutosaveSightings& sightings);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
