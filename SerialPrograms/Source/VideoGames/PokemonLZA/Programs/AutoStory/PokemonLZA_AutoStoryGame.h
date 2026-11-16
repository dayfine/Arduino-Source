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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryRowExecution.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Every action a PLZA checkpoint may name, GAME actions but for "talk":
//
//      "confirm_new_game"      console_only: 01.00's first step, on the new
//                              game's "Confirm Choices" (confirm_new_game_choices())
//      "opening_to_platform"   the opening cutscene until the autosave badge
//                              has shown and the station platform holds
//      "wait_for_platform"     the station platform holds: a start check
//      "walk"                  args "legs", [[turn, ms], ...] as
//                              story_walk_move() reads them, "held",
//                              "wait_ms", and "end",
//                              "fade_white", "fade_black" or "dialog";
//                              walked once the screen shows free control
//                              (check_can_walk()), until its end event if it
//                              has one (walk_until()), else until its legs
//                              have run (finish_walk())
//      "mash_to_move_hints"    A through scenes until the player has control
//                              (run_scene_machine()): 01.01's arrival; args
//                              "min_length_ms", no quiet failure before it,
//                              "quiet_ms", "quiet_is_control" and
//                              "press_a_first" (press_through_prompt())
//      "walk_to_prompt"        straight on until an A prompt shows
//      "text_to_choice"        B through a scene's text until its choice box
//                              shows (run_scene_machine()); A with
//                              "advance_with_a"
//      "choose_0", "choose_1"  that row of the choice box on screen (0 the
//                              top), by the D-pad from the highlighted row,
//                              then A
//      "talk" (TALK)           A at what the player faces, then its text box
//      "a" (MASH, "scene")     A through a scene until control; it must
//                              start within 45 s
//      "story_battle_start"    A through the talk before a story battle and
//                              the battle tutorial's cards until the battle
//                              HUD (run_scene_machine())
//      "story_battle"          the battle, by the backend's battle()
//
//  Later slices add rows here, each with its handler.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine
//  (StoryActionRegistry::lint), then with a walk's "end".
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The event a walk's "end" arg names: "fade_white", "fade_black" or
//  "dialog". None for any other word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  PLZA as a StoryGame on the console `env`, which must outlive it. The
//  save counts the game's autosave at the end of the "autosave" rows of
//  `rows`; after the others nothing saves (Pokemon::NOT_SAVED, "save
//  unverified" for the "unverified" ones). The reload resets the game from
//  HOME onto the latest save.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game(StoryEnv& env, const Pokemon::StoryRows& rows);

//  The same with no console, for tests and simulations: the console_only
//  actions fail as "console_only", and there is no reload.
//  code-health: allow test_only_api -- a test double: the game the scripted tests and simulations run
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game_without_console(const Pokemon::StoryRows& rows);

//  Loads the save `row` starts from with `game`'s reload (the latest
//  save), then, unless `row` starts on the station platform (01.01, whose
//  first step checks it), looks a few times: the platform on
//  PLATFORM_LOOKS of them in a row is 01.01's start, a save older than
//  `row`'s, and fails as "older_save". Fails as "no_reload" for a game with
//  no reload.
Pokemon::Outcome load_checkpoint_start(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& row
);

//  `rows` in order from the save the game is on, as
//  StoryEngine::run_segment() runs them, `attempts_per_checkpoint` each,
//  cut at the rows `story` marks reload_when_chained
//  (Pokemon::story_run_segments()): before each, the event
//  "reload_when_chained" loads its start with load_checkpoint_start(). A
//  failed load fails the run there.
Pokemon::SegmentReport run_plza_segments(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, const Pokemon::StoryRows& story,
    std::span<const Pokemon::StoryCheckpoint> rows, size_t attempts_per_checkpoint,
    Pokemon::StoryEngine<Pokemon::ZonePose>& engine, PlzaBackend& backend, Pokemon::RunState& state
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
