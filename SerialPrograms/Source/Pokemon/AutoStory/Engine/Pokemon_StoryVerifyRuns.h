/*  Story Verify Runs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Verify mode (verify_checkpoint()) and verification-loop rows
 *  (run_verify_row(), e.g. FRLG League 20.01-20.05) implemented as free
 *  functions over the shared StoryEngine::run_checkpoints() (E4), using a
 *  no-save copy of StoryGame so core engine signatures remain unchanged.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryVerifyRuns_H
#define PokemonAutomation_Pokemon_StoryVerifyRuns_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryVerify.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Builds a fresh StoryCheckpoint for one verify try.
using StoryCheckpointFactory = MoveOnlyFunction<StoryCheckpoint()>;

//  Optional pass predicate checked after a try's steps complete and before
//  any save (e.g. postconditions + FRLG League check_league_save).
//  std::nullopt = passed; VERIFY_UNREAD stops the loop immediately with no
//  save; any other failure counts the try as failed (not passed).
using VerifyPassPredicate = MoveOnlyFunction<std::optional<StoryFailure>(
    StoryBackend& backend, const StoryCheckpoint& passed, RunState& state
)>;

//  Policy for Verify mode (verify_checkpoint()).
struct VerifyModePolicy{
    size_t runs = 3;
    size_t passes_needed = 2;
    StorySaveMode save_mode = StorySaveMode::NONE;
};

//  Outcome of verify_checkpoint().
struct VerifyModeReport{
    std::string checkpoint = {};
    size_t runs = 0;
    size_t passes = 0;
    bool verified = false;
    bool saved = false;
    //  Set when the verify loop stopped early (bad policy, failed reload,
    //  repeating/fatal failure).
    std::optional<StoryFailure> failure = std::nullopt;
};

//  Outcome of run_verify_row() (two-phase verification loop).
struct VerifyRowReport{
    std::string checkpoint = {};
    size_t attempts = 0;
    size_t reloads = 0;
    size_t phase1_passes = 0;
    bool passed = false;
    bool saved = false;
    std::optional<CheckpointFailure> failure = std::nullopt;
};

//  Runs a single checkpoint `policy.runs` times from its start save via
//  run_checkpoints() with a no-save copy of `game`, reloading via
//  `game.reload` between runs. Saves once on the final run only when
//  `policy.save_mode == AFTER_VERIFICATION` and every run passed.
VerifyModeReport verify_checkpoint(
    StoryBackend& backend,
    std::string_view checkpoint_id,
    const VerifyModePolicy& policy,
    StoryCheckpointFactory make_checkpoint,
    StoryGame& game,
    RunState& state,
    VerifyPassPredicate verify_pass = {}
);

//  Two-phase verification loop for a row with StoryVerify (FRLG League):
//  - Phase 1: runs up to `verify.tries` times without saving (via
//    run_checkpoints() with a no-save copy of `game`). Stops early once
//    `verify.min_pass` tries pass, or stops with `verify_below_min_pass` as
//    soon as remaining tries cannot reach `min_pass`.
//  - Phase 2: runs up to `verify.reset_budget` times, saving the first try
//    that passes (via `game.save`) or stopping with `verify_out_of_budget`.
//  Stops immediately without reloading on any `failure.repeats` failure
//  (GAME_NOT_RUNNING, precondition_failed, start_failed) or VERIFY_UNREAD.
VerifyRowReport run_verify_row(
    StoryBackend& backend,
    std::string_view checkpoint_id,
    const StoryVerify& verify,
    StoryCheckpointFactory make_checkpoint,
    StoryGame& game,
    RunState& state,
    VerifyPassPredicate verify_pass = {}
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
