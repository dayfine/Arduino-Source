/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA as the shared engine's StoryGame, and the run of checkpoints on it:
 *  between two checkpoints the run checks it is at the next one's start,
 *  or reloads the save there. The actions the checkpoints' steps name are
 *  in Actions/PokemonLZA_AutoStoryActionTable.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGame_H
#define PokemonAutomation_PokemonLZA_AutoStoryGame_H

#include <optional>
#include <set>
#include <span>
#include <string>
#include "Common/Cpp/MoveOnlyFunction.h"
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


//  PLZA as a StoryGame, saving per autosave_policy() on what `sightings`
//  (which must outlive it) saw; no reload.
Pokemon::StoryGame plza_story_game(const MarkedCheckpoints& marked, AutosaveSightings& sightings);

//  Reloads the latest save. Fails as "no_reload" when `game` has none,
//  "reload_failed" when the reload says it is at no start.
std::optional<Pokemon::StoryFailure> load_checkpoint_start(
    Pokemon::StoryGame& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& checkpoint
);

//  Reads the screen for the fields `start` names.
using StartReader = MoveOnlyFunction<StartSignature(const StartSignature& start)>;

//  How run_plza_checkpoints() runs its checkpoints.
struct CheckpointRunOptions{
    size_t attempts_per_checkpoint = 1;
    std::set<std::string, std::less<>> reload_when_chained = {};
    std::span<const CheckpointStart> starts = {};
    //  Unset: no start can be checked.
    StartReader read_start = {};
};

//  Runs `checkpoints` in order from the current save; none when all passed. A checkpoint
//  is retried only when a reload returns to its start (first checkpoint, reloaded
//  checkpoint, or previous autosave seen), and not once it autosaved. Between
//  checkpoints, the "chain" (a failure with no event index):
//  - reload_when_chained checkpoints reload and must land at their start:
//    "reload_off_start", or "reload_unchecked" with no start or reader.
//  - other checkpoints are checked on the live screen: "chain_off_start" when
//    elsewhere; no start or reader runs on unchecked.
std::optional<Pokemon::CheckpointFailure> run_plza_checkpoints(
    Pokemon::StoryGame& game, std::span<Pokemon::StoryCheckpoint> checkpoints, CheckpointRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
