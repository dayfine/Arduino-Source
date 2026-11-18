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


//  Every action a PLZA checkpoint may name, with its args and their why
//  strings; each handler's comment in the .cpp says what it does.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  What is wrong with `checkpoint` for PLZA's engine
//  (StoryActionRegistry::lint), then with a walk's "end", then a talk not
//  right after a step that ends on its A prompt (a walk to "prompt"): a
//  talk never presses A blind.
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The event a walk's "end" arg names: "fade_white", "fade_black",
//  "dialog" or "prompt" (INTERACT_PROMPT). None for any other word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  The handlers of the console_only actions, by name.
using PlzaConsoleActions = std::map<std::string, Pokemon::StoryActionHandler<Pokemon::ZonePose>, std::less<>>;

//  PLZA as a StoryGame, with no reload: the console_only actions by
//  `console`, one it lacks failing as "console_only" (none: tests and
//  simulations). Every step's looks go
//  to `sightings`, which must outlive it; the save at a checkpoint's end
//  counts the game's autosave when its badge was seen there ("[Save] <id>:
//  the game autosaved (seen)"), else nothing saved (Pokemon::NOT_SAVED,
//  "save unverified" for the plan's "unverified" rows). A walk's "heading"
//  is faced by `arrow`; with none, such a walk fails as "console_only".
Pokemon::StoryGame<Pokemon::ZonePose> plza_story_game(
    const PlzaConsoleActions& console, const Pokemon::StoryRows& rows, std::optional<MinimapArrowReader> arrow,
    AutosaveSightings& sightings
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
