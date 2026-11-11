/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA for the shared story engine (Pokemon/AutoStory/Engine/): its
 *  actions as a StoryGame<ZonePose>, and their specs, which a checkpoint's
 *  JSON is read against. It replaces PLZA's own story executor.
 *
 *  The actions that read or press through the game's own screens (menus,
 *  the naming keyboard, the battle AI, fast travel) are console_only: the
 *  engine refuses them elsewhere. The scenes of 01.00 and the walks run on
 *  any backend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"

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
//      ROUTE   "fly"       fly to "place", a fast travel place's slug
//                          ("vert-pokemon-center")
//      TALK    "talk"      A at what the player faces
//      ANSWER  "answer"    the first choice for yes, the second for no
//      MASH    "mash"      B through the text until the overworld is back
//      MASH    "a"         A through it
//      GAME    "walk" (args "legs", [[turn, ms], ...] as story_walk_move()
//              reads them; "end", "fade_white" or "fade_black"; "heading"),
//              walked once the screen shows free control
//              (check_can_walk()), until its end fade if it has one
//              (walk_until()); the platform waits "wait_for_platform" and
//              "opening_to_platform"; the scene machines
//              "mash_to_move_hints" (args min_length_ms, quiet_ms) and
//              "text_to_choice" (run_scene_machine()); a console step, one
//              call through the game's own screens with no args: the new
//              game's "new_game_language", "pick_look", "type_player_name";
//              "choose_0" / "choose_1" (that row of the choices on screen);
//              or an action of story_actions_not_implemented()
//
//  Every action that works through the game's own screens is console_only.
//  BATTLE is the built-in "fight" (the backend's battle), SAVE the
//  engine's, and the built-in "route" and "shop" fail by name. Lumiose has
//  no tiles: a step naming "legs" or "face" fails the lint.
std::vector<Pokemon::StoryActionSpec> plza_story_action_specs();

//  Every action a PLZA checkpoint may name: the built-ins with
//  plza_story_action_specs() on top, as the engine registers them. Its JSON
//  is read against these.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine
//  (StoryActionRegistry::lint), then with its steps' args
//  (plza_step_arg_errors(), PokemonLZA_AutoStoryStepValidator.h).
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The walk of `args`: the legs of its arg `legs` ("legs" for a walk), each [turn in degrees clockwise
//  (negative: left), walk ms], and its "heading", if any. The args must
//  have passed plza_story_lint().
StoryMove story_walk_move(const Pokemon::StoryActionArgs& args, std::string_view legs);

//  PLZA as a StoryGame on the console `env`, which must outlive it.
//  The walks read the
//  compass with heading_reader(). The save waits out the game's autosave at
//  the end of the checkpoints of `autosave_rows` (the run's
//  StoryData::autosaves) and saves through the menu after the others; the
//  reload resets the game from HOME.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game(
    StoryEnv& env, std::set<std::string, std::less<>> autosave_rows
);

//  The same with no console, for tests and simulations: the console_only
//  actions fail as "console_only", the walks read the heading the looks
//  carry, and there is no save or reload.
Pokemon::StoryGame<Pokemon::ZonePose> make_offline_plza_story_game();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
