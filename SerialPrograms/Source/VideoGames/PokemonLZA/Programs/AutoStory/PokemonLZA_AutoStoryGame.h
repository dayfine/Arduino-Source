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
#include "PokemonLZA_AutoStoryAutosaves.h"
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
//      "talk_through_map_screen"   A and B in turn through Taunie's talk
//                              and the first map (03.02)
//      "climb_ladder"          the side street's ladder (03.03)
//      "talk_to_pokedex_screen"    B through the nurse's talk and the
//                              Pokedex app (03.04)
//      "run_to_gate"           console_only: straight on to a zone gate's A
//                              prompt (upstream's
//                              run_towards_gate_with_A_button()), args
//                              "timeout_ms" and "heading", the minimap
//                              heading faced first
//      "leave_zone_gate"       console_only: A through a zone's gate on the
//                              way out (upstream's leave_zone_gate())
//      "catch_in_zone"         console_only: 03.08's patrol of Wild Zone 1's
//                              road until the party bar shows 5 species
//      "walk_back_to_zone_road"    console_only: back onto Wild Zone 1's
//                              road at the catch spot by the far gate
//
//  Later slices add rows here, each with its handler.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine
//  (StoryActionRegistry::lint), then with a walk's "end".
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The event a walk's "end" arg names: "fade_white", "fade_black" or
//  "dialog". None for any other word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  PLZA as a StoryGame on the console `env`, which must outlive it, as must
//  `sightings`. Every step's looks go to `sightings`; the save at a
//  checkpoint's end counts the game's autosave when its badge was seen
//  there ("[Save] <id>: the game autosaved (seen)"), else nothing saved
//  (Pokemon::NOT_SAVED, "save unverified" for the plan's "unverified"
//  rows). The reload resets the game from HOME onto the latest save.
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game(
    StoryEnv& env, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
);

//  The same with no console, for tests and simulations: the console_only
//  actions fail as "console_only", and there is no reload.
//  code-health: allow test_only_api -- a test double: the game the scripted tests and simulations run
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game_without_console(
    const Pokemon::StoryRows& rows, AutosaveSightings& sightings
);

//  Loads the save `row` starts from with `game`'s reload (the latest
//  save), then, unless `row` starts on the station platform (01.01, whose
//  first step checks it), looks a few times: the platform on
//  PLATFORM_LOOKS of them in a row is 01.01's start, a save older than
//  `row`'s, and fails as "older_save". Fails as "no_reload" for a game with
//  no reload.
Pokemon::Outcome load_checkpoint_start(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& row
);

//  `rows` in order from the save the game is on, each with the story
//  engine's run_segment() on `backend`, `attempts_per_checkpoint` each when a reload
//  returns to its start (the first row, or one after a row whose autosave
//  `sightings` saw), else once. Between two rows, the event "chain":
//    - a row `story` marks reload_when_chained "always" loads its start
//      (load_checkpoint_start()) when the row before's autosave was seen;
//    - any other row runs on from the live screen when its start check
//      passes or cannot tell (logged as "start unchecked"); when it fails,
//      the row's start is loaded if the row before's autosave was seen.
//  With no autosave seen, a reload would rewind to an older save: the run
//  fails there as "no_start_save". A failed load fails the run too.
Pokemon::SegmentReport run_plza_segments(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, const Pokemon::StoryRows& story,
    std::span<const Pokemon::StoryCheckpoint> rows, size_t attempts_per_checkpoint,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
