/*  Checkpoint Retry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  SV's checkpoint retry policy on the shared loop (Pokemon_CheckpointAttempts.h):
 *  an attempt fails by throwing OperationFailedException; on a failure SV
 *  resets the game to the checkpoint's save and tries again, reports once
 *  for debugging after some failures, and gives up after the last attempt.
 *  The game supplies what it does at each step; the policy, the counters
 *  and the log lines (Pokemon_StoryLog.h) live here.
 *
 */

#ifndef PokemonAutomation_Pokemon_CheckpointRetry_H
#define PokemonAutomation_Pokemon_CheckpointRetry_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_AutoStoryStats.h"
#include "Pokemon_CheckpointAttempts.h"

namespace PokemonAutomation{
namespace Pokemon{


//  SV's policy: how often a checkpoint retries and when it reports.
struct CheckpointRetryPolicy{
    //  Attempts before the checkpoint gives up.
    size_t max_attempts;
    //  The failure count at which the game reports once for debugging.
    size_t report_at_failure;
};

//  A game step that takes nothing: a reset, a stats update.
using CheckpointStep = std::function<void()>;
//  A game step on the failure that reached a policy count.
using CheckpointFailureStep = std::function<void(const OperationFailedException& error)>;

//  What SV does at each step of the loop, where a failed attempt throws.
struct CheckpointRetryActions{
    //  Runs attempt `attempt`, counting from 1. Throws
    //  OperationFailedException when the attempt fails.
    std::function<void(size_t attempt)> run;
    //  Sends the debugging report, with the exception of the failure that
    //  reached the policy's count.
    CheckpointFailureStep report;
    //  Throws the game's final error after the last attempt failed.
    CheckpointFailureStep give_up;
    //  Resets the game to the checkpoint's save.
    CheckpointStep reset;
    //  Publishes changed stats.
    CheckpointStep update_stats;
    CheckpointLogger log;
};

//  Runs `checkpoint` through run_checkpoint_attempts() with the start,
//  attempt and reset lines, until an attempt passes, and returns that
//  attempt's number. After the last attempt fails it calls `give_up` and
//  rethrows. Each reset adds to `stats.m_reset`.
size_t run_checkpoint_with_retries(
    std::string_view checkpoint,
    const CheckpointRetryPolicy& policy,
    const CheckpointRetryActions& actions,
    AutoStoryStats& stats
);


//  The status message sent after the game saved at a checkpoint.
inline constexpr std::string_view CHECKPOINT_SAVED_MESSAGE = "Saved at checkpoint.";

//  Counts a checkpoint the game just saved at, publishes the stats and
//  sends CHECKPOINT_SAVED_MESSAGE through `notify`.
void record_checkpoint_saved(
    const CheckpointStep& update_stats,
    const std::function<void(std::string_view message)>& notify,
    AutoStoryStats& stats
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
