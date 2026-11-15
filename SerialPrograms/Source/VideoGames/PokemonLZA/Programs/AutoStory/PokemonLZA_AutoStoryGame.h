/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA for the shared story engine (Pokemon/AutoStory/Engine/): its
 *  actions as a StoryGame<ZonePose>, and their specs, which a checkpoint's
 *  JSON is read against.
 *
 *  The actions that read or press through the game's own screens are
 *  console_only: the engine refuses them elsewhere. The new game's opening,
 *  the walks and the scene machine run on any backend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Every action a PLZA checkpoint may name, all GAME actions:
//
//      "confirm_new_game"      console_only: 01.00's first step, on the new
//                              game's "Confirm Choices" (confirm_new_game_choices())
//      "opening_to_platform"   the opening cutscene until the autosave badge
//                              has shown and the station platform holds
//      "wait_for_platform"     the station platform holds: a start check
//      "walk"                  args "legs", [[turn, ms], ...] as
//                              story_walk_move() reads them, and "end",
//                              "fade_white", "fade_black" or "dialog";
//                              walked once the screen shows free control
//                              (check_can_walk()), until its end event if it
//                              has one (walk_until()), else until its legs
//                              have run (finish_walk())
//      "mash_to_move_hints"    A through scenes until the player has control
//                              (run_scene_machine()): 01.01's arrival
//
//  Later slices add rows here, each with its handler.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine
//  (StoryActionRegistry::lint), then with a walk's "end".
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The event a walk's "end" arg names: "fade_white", "fade_black" or
//  "dialog". None for any other word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  The walk of `args`: the legs of its arg "legs", each [turn in degrees
//  clockwise (negative: left), walk ms]. The args must have passed the
//  walk's spec.
StoryMove story_walk_move(const Pokemon::StoryActionArgs& args);

//  PLZA as a StoryGame on the console `env`, which must outlive it. The
//  save waits out the game's autosave at the end of the checkpoints of
//  `autosave_rows` (the story's StoryData::autosaves); after the others
//  nothing saves, and it fails as "no_autosave". The reload resets the game
//  from HOME onto the latest save.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game(
    StoryEnv& env, std::set<std::string, std::less<>> autosave_rows
);

//  The same with no console, for tests and simulations: the console_only
//  actions fail as "console_only", and there is no save or reload.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game_without_console();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
