/*  Story Save Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Phase-gated save policy wrapper (S12, E6) over StoryGame::save: reads a
 *  checkpoint's testing status (testing, verification, reproduction) and the
 *  run's StorySaveMode, and gates whether the inner save policy may run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySavePolicy_H
#define PokemonAutomation_Pokemon_StorySavePolicy_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/EnumStringMap.h"
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How far a checkpoint has been proven (plan's checkpoint.metadata.phase).
enum class CheckpointTestingStatus{
    TESTING,
    VERIFICATION,
    REPRODUCTION,
};

//  Wire names ("testing", "verification", "reproduction") for
//  CheckpointTestingStatus.
const EnumStringMap<CheckpointTestingStatus>& CHECKPOINT_TESTING_STATUS_NAMES();

//  The save mode a checkpoint's testing status allows on its own:
//  TESTING -> NONE, VERIFICATION -> AFTER_VERIFICATION,
//  REPRODUCTION -> PER_CHECKPOINT.
StorySaveMode testing_status_save_mode(CheckpointTestingStatus status);

//  The stricter of the run's requested save mode and the checkpoint's testing
//  status save mode (NONE < AFTER_VERIFICATION < PER_CHECKPOINT).
StorySaveMode effective_save_mode(StorySaveMode requested, CheckpointTestingStatus status);

//  A game's save policy callback (same signature as StoryGame::save).
using StorySavePolicy = MoveOnlyFunction<std::optional<StoryFailure>(
    StoryBackend& backend, const StoryCheckpoint& passed
)>;

//  Per-checkpoint testing status lookup used by phase_gated().
struct PhaseGateConfig{
    //  The run's requested save mode (e.g. from StoryRunOptions::SAVE_MODE).
    StorySaveMode requested_mode = StorySaveMode::NONE;
    //  The id of the last checkpoint in the run (SavePoint::RUN_END).
    std::string last_checkpoint_id = {};
    //  Each checkpoint's testing status by id; absent ids default to TESTING.
    std::map<std::string, CheckpointTestingStatus, std::less<>> status_by_id = {};
    //  If true, ignore status_by_id and apply requested_mode directly.
    bool ignore_phase = false;
};

//  Wraps `inner` with phase and StorySaveMode gating. When the effective save
//  mode forbids saving at `passed`'s SavePoint, logs why and returns
//  std::nullopt without calling `inner`.
StorySavePolicy phase_gated(StorySavePolicy inner, PhaseGateConfig config);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
