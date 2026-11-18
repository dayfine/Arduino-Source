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
#include <map>
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
//      "new_game_intro"        console_only: 01.00's first step, from HOME or
//                              the language list to "Confirm Choices"
//                              (play_new_game_intro())
//      "confirm_new_game"      console_only: on the new game's "Confirm
//                              Choices" (confirm_new_game_choices())
//      "opening_to_platform"   the opening cutscene until the autosave badge
//                              has shown and the station platform holds
//      "wait_for_platform"     the station platform holds: a start check
//      "walk"                  args "legs", [[turn, ms], ...] as
//                              story_walk_move() reads them, "held",
//                              "wait_ms", "end_after_ms" (no end before
//                              that much of the last leg), and "end",
//                              "fade_white", "fade_black", "dialog" or
//                              "prompt" (an A prompt), and "heading", the
//                              minimap heading faced (face_heading())
//                              before the legs;
//                              walked once the screen shows free control
//                              (check_can_walk()), until its end event if it
//                              has one (walk_until()), else until its legs
//                              have run (finish_walk())
//      "mash_to_move_hints"    B through scenes' text (A at a choice) until
//                              the player has control
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
//      "talk" (TALK)           on the A prompt of what the player faces
//                              (none: nothing pressed, "talk_no_prompt"),
//                              A, then its scene under way
//                              (scene_under_way(): text, a subtitle, a
//                              card, a fade or a textless cutscene)
//      "a" (MASH, "scene")     B through a scene's text (A at a choice)
//                              until control; it must
//                              start within 45 s
//      "story_battle_start"    A through the talk before a story battle and
//                              the battle tutorial's cards until the battle
//                              HUD (run_scene_machine())
//      "story_battle"          the battle, by the backend's battle(), on
//                              the battle HUD within 10 s, else
//                              "battle_never_started"; right after
//                              "story_battle_start" (the lint)
//      "talk_through_map_screen"   A on Taunie's talk and the Map &
//                              Missions card, B on the first map (03.02b)
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
//      "catch_in_zone"         console_only: 03.08's way from Taunie's rocks
//                              to the road's spot at 100, then a search
//                              there until the party bar shows 5 species
//      "walk_back_to_zone_road"    console_only: back onto Wild Zone 1's
//                              road at the catch spot by the far gate
//
//  Later slices add rows here, each with its handler.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine
//  (StoryActionRegistry::lint), then with a walk's "end", then a talk not
//  right after a step that ends on its A prompt (a walk to "prompt", or
//  "walk_to_prompt"): a talk never presses A blind.
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The event a walk's "end" arg names: "fade_white", "fade_black",
//  "dialog" or "prompt" (INTERACT_PROMPT). None for any other word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  The handlers of the console_only actions, by name.
using PlzaConsoleActions = std::map<std::string, Pokemon::StoryActionHandler<Pokemon::ZonePose>, std::less<>>;

//  PLZA as a StoryGame, with no reload: the console_only actions by
//  `console`, one it lacks failing as "console_only". Every step's looks go
//  to `sightings`, which must outlive it; the save at a checkpoint's end
//  counts the game's autosave when its badge was seen there ("[Save] <id>:
//  the game autosaved (seen)"), else nothing saved (Pokemon::NOT_SAVED,
//  "save unverified" for the plan's "unverified" rows). A walk's "heading"
//  is faced by `arrow`; with none, such a walk fails as "console_only".
Pokemon::StoryGame<Pokemon::ZonePose> plza_story_game(
    const PlzaConsoleActions& console, const Pokemon::StoryRows& rows, AutosaveSightings& sightings,
    std::optional<MinimapArrowReader> arrow
);

//  plza_story_game() with no console, for tests and simulations: the
//  console_only actions fail as "console_only".
//  code-health: allow test_only_api -- a test double: the game the scripted tests and simulations run
Pokemon::StoryGame<Pokemon::ZonePose> make_plza_story_game_without_console(
    const Pokemon::StoryRows& rows, AutosaveSightings& sightings,
    std::optional<MinimapArrowReader> arrow = std::nullopt
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

//  How run_plza_segments() runs its rows.
struct SegmentRunOptions{
    size_t attempts_per_checkpoint = 1;
};

//  `rows` in order from the save the game is on, each with the story
//  engine's run_segment() on `backend`, `options.attempts_per_checkpoint` each when a reload
//  returns to its start (the first row, or one after a row whose autosave
//  `sightings` saw), else once. Between two rows, the event "chain": a row
//  runs on from the live screen when its start check passes or cannot
//  tell (logged as "start unchecked"); when it fails, the row's start is
//  loaded (load_checkpoint_start()) if the row before's autosave was seen,
//  with a fresh `state`.
//  With no autosave seen, a reload would rewind to an older save: the run
//  fails there as "no_start_save". A failed load fails the run too.
//  A row that saw an autosave and then failed is not retried: the save
//  moved past its start, so its reload fails as "save_moved", and the run
//  logs "[Save] <id>: the game autosaved in <step> before the run failed".
Pokemon::SegmentReport run_plza_segments(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game,
    std::span<const Pokemon::StoryCheckpoint> rows, const SegmentRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
