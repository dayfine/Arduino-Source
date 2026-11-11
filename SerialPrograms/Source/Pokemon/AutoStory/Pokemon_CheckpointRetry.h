/*  Checkpoint Retry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The retry policy of a game whose AutoStory runs each checkpoint in its
 *  own loop (SV's): run the checkpoint, and on an operation failure reset
 *  the game to the checkpoint's save and try again, report once for
 *  debugging after some failures, and give up after the last attempt.
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

namespace PokemonAutomation{
namespace Pokemon{


//  How often a checkpoint retries and when it reports.
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
//  Writes one log line.
using CheckpointLogger = std::function<void(const std::string& line)>;

//  What the game does at each step of the loop.
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

//  Runs `checkpoint` until an attempt passes, and returns that attempt's
//  number. Each reset adds to `stats.m_reset`.
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
