/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's actions for the shared story engine, each parsing its own args
 *  from a checkpoint's JSON. Actions that read the game's own screens run
 *  only with a console's tools; walks and the scene machine run on any
 *  backend.
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
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The event a walk's "end" arg names; none for "scene" or an unknown word.
std::optional<ScreenEvent> walk_end_event(std::string_view end);

//  Console actions by name. `timeout` is the step's "timeout_ms": the gate
//  runs read it, the others ignore it.
using PlzaConsoleActions = std::map<
    std::string, std::function<std::optional<Pokemon::StoryFailure>(PlzaBackend& backend, Milliseconds timeout)>, std::less<>
>;

//  What PLZA's actions use besides the backend. Off a console all are empty:
//  a console action fails as CONSOLE_ONLY, a walk "heading" too, and a
//  battle as "no_battle_code".
struct PlzaStoryTools{
    PlzaConsoleActions console = {};
    std::optional<MinimapArrowReader> arrow = std::nullopt;
    std::optional<LockOnWatch> lock_on = std::nullopt;
    //  Fights the battle on screen until it ends.
    std::function<std::optional<Pokemon::StoryFailure>(PlzaBackend& backend)> battle = {};
    //  Where a menu save counts as an autosave; none: nowhere.
    AutosaveSightings* sightings = nullptr;
};

//  Every action a PLZA checkpoint may name, bound to `tools`, which must
//  outlive the steps parsed with them.
Pokemon::StoryActions plza_story_actions(const PlzaStoryTools& tools);

//  PLZA as a StoryGame, saving per autosave_policy() on what `sightings`
//  (which must outlive it) saw; no reload.
Pokemon::StoryGame plza_story_game(const StoryRows& rows, AutosaveSightings& sightings);

//  Reloads the latest save. Fails as "older_save" when it lands on the
//  station platform but `row` is not 01.01, "no_reload" when `game` has
//  none, "reload_failed" when the reload says it is at no start.
std::optional<Pokemon::StoryFailure> load_checkpoint_start(
    Pokemon::StoryGame& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& row
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
    //  Dev (Reload Probe): after each of these rows passes, reload once, log
    //  where it lands with `log_landing`, and go on from the row whose start
    //  the screen shows.
    std::set<std::string, std::less<>> reload_probe_after = {};
    std::function<void(const Pokemon::StoryCheckpoint& passed)> log_landing = {};
};

//  Runs `rows` in order from the current save; none when all passed. A row
//  is retried only when a reload returns to its start (first row, reloaded
//  row, or previous autosave seen), and not once it autosaved. Between
//  rows, the "chain" (a failure with no event index):
//  - reload_when_chained rows reload and must land at their start:
//    "reload_off_start", or "reload_unchecked" with no start or reader.
//  - other rows are checked on the live screen: "chain_off_start" when
//    elsewhere; no start or reader runs on unchecked. On the station
//    platform, the row's start is reloaded, "no_start_save" when no
//    autosave was seen.
std::optional<Pokemon::CheckpointFailure> run_plza_segments(
    Pokemon::StoryGame& game, std::span<Pokemon::StoryCheckpoint> rows, const SegmentRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
