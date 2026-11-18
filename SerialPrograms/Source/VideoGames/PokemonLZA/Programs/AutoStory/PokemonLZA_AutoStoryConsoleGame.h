/*  PokemonLZA AutoStory - Story game on the console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's story game (PokemonLZA_AutoStoryGame.h) with the console_only
 *  actions and the reload, which need the console.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleGame_H

#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  plza_story_game() on the console `env`, which must outlive it, as must
//  `sightings`: its console_only actions run there, walks face their
//  heading by its minimap arrow, and the reload resets the game from HOME
//  onto the latest save.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game(
    StoryEnv& env, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
