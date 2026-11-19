/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's actions for the shared story engine, and the specs a checkpoint's
 *  JSON is read against. Actions that read the game's own screens are
 *  console_only; walks and the scene machine run on any backend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <functional>
#include <map>
#include <optional>
#include <set>
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
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Every action a PLZA checkpoint may name, with its args.
const std::vector<Pokemon::StoryActionSpec>& plza_story_specs();

//  Also flags a talk not right after a step that ends on its A prompt: a
//  talk never presses A blind.
std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint);

//  The event a walk's "end" arg names; none for "scene" or an unknown word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  The handlers of the console_only actions, by name.
using PlzaConsoleActions = std::map<std::string, Pokemon::StoryActionHandler<Pokemon::ZonePose>, std::less<>>;

//  PLZA as a StoryGame with no reload. A console_only action missing from
//  `console`, or a walk "heading" with no `arrow`, fails as "console_only".
//  `sightings` must outlive the game; a checkpoint's end save counts only
//  when the autosave badge was seen there, else Pokemon::NOT_SAVED.
Pokemon::StoryGame<Pokemon::ZonePose> plza_story_game(
    const PlzaConsoleActions& console, const Pokemon::StoryRows& rows, std::optional<MinimapArrowReader> arrow,
    std::optional<LockOnWatch> lock_on, AutosaveSightings& sightings
);

//  Reloads the latest save. Fails as "older_save" when it lands on the
//  station platform but `row` is not 01.01, "no_reload" when `game` has none.
Pokemon::Outcome load_checkpoint_start(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& row
);

//  How run_plza_segments() runs its rows.
struct SegmentRunOptions{
    size_t attempts_per_checkpoint = 1;
    std::set<std::string, std::less<>> reload_when_chained = {};
    std::span<const CheckpointStart> starts = {};
    //  Reads the screen for the fields `start` names. Unset: no start can be checked.
    std::function<StartSignature(const StartSignature& start)> read_start = {};
    //  After the chain into a row, before its first step.
    std::function<void(const Pokemon::StoryCheckpoint& row)> at_start = {};
};

//  Runs `rows` in order from the current save. A row is retried only when a
//  reload returns to its start (first row, reloaded row, or previous
//  autosave seen). Between rows, the "chain" event:
//  - reload_when_chained rows reload and must land at their start:
//    "reload_off_start", or "reload_unchecked" with no start or reader.
//  - other rows are checked on the live screen: "chain_off_start" when
//    elsewhere; no start or reader runs on unchecked. On the station
//    platform, the row's start is reloaded, "no_start_save" when no
//    autosave was seen.
//  A row that autosaved and then failed is not retried ("save_moved").
Pokemon::SegmentReport run_plza_segments(
    const Pokemon::StoryGame<Pokemon::ZonePose>& game,
    std::span<const Pokemon::StoryCheckpoint> rows, const SegmentRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
