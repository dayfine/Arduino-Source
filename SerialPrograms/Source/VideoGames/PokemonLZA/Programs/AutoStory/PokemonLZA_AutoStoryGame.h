/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA for the shared story engine (Pokemon/AutoStory/Engine/): its
 *  actions as a StoryGame<ZonePose>, and their specs, which a checkpoint's
 *  JSON is read against. It replaces PLZA's own story executor.
 *
 *  The actions that read or press through the game's own screens (menus,
 *  the naming keyboard, the battle AI, fast travel) run only on a console
 *  (PlzaStoryBackend::console_env()); elsewhere they fail as
 *  "console_only". The scenes of 01.00 and the walks run on any backend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The GAME actions whose code does not exist yet, by action, each with the
//  code it waits for. They throw StoryActionNotImplemented with that text,
//  which ends the run, instead of pressing buttons blind.
const std::map<std::string, std::string, std::less<>>& story_actions_not_implemented();

//  PLZA's own actions and its replacements of built-in ones (same kind and
//  name), in the order the game registers them:
//
//      ROUTE   "fly"       fly to the target, whose map is a fast travel
//                          place's slug ("vert-pokemon-center")
//      LEGS    "legs"      fails: Lumiose has no tiles
//      STEP    "face"      fails: Lumiose has no tiles
//      TALK    "talk"      A at what the player faces
//      ANSWER  "answer"    the first choice for yes, the second for no
//      MASH    "mash"      B through the text until the overworld is back
//      MASH    "a"         A through it
//      HEAL    "heal"      the nurse (not implemented)
//      SHOP    "shop"      the counter (not implemented)
//      GAME    a move of story_moves(), walked once the screen shows free
//              control (check_can_walk()), until its end event if it has
//              one (story_move_ends()); "approach_starter" (the walk of
//              starter_moves() to the chosen starter); "choose_0" /
//              "choose_1" (that row of the choices on screen);
//              "story_battle" (attack in a battle already on screen until a
//              dialog shows); the new game's "new_game_language",
//              "pick_look", "type_player_name"; the scene machines
//              "mash_to_move_hints" (args min_length_ms, quiet_ms) and
//              "text_to_choice" (run_scene_machine()); the scene mashes
//              "story_battle_start", "talk_through_map_screen",
//              "talk_to_pokedex_screen" (mash_scene()); "climb_ladder"; or
//              an action of story_actions_not_implemented()
//
//  BATTLE is the built-in "fight" (the backend's battle), SAVE the
//  engine's, and the built-in "route" fails by name.
std::vector<Pokemon::StoryActionSpec> plza_story_action_specs();

//  Every action a PLZA checkpoint may name: the built-ins with
//  plza_story_action_specs() on top, as the engine registers them. Its JSON
//  is read against these.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine (StoryActionRegistry::lint).
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  PLZA as a StoryGame on `backend`, which must outlive it and be the
//  backend the engine runs on: its actions use PLZA's inputs (turn(),
//  heading()) and console. `starter` is the one approach_starter walks to.
//  The save waits out the game's autosave at the end of the checkpoints
//  that rely on one (story_autosave_rows()) and saves through the menu
//  after the others; the reload resets the game from HOME.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game(PlzaStoryBackend& backend, StarterChoice starter);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
