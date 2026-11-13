/*  Checkpoint Retry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryLog.h"
#include "Pokemon_CheckpointRetry.h"

namespace PokemonAutomation{
namespace Pokemon{


size_t run_checkpoint_with_retries(
    std::string_view checkpoint,
    const CheckpointRetryPolicy& policy,
    const CheckpointRetryActions& actions,
    AutoStoryStats& stats
){
    actions.log(checkpoint_start_line(checkpoint));
    const CheckpointAttempts attempts{
        .max_attempts = policy.max_attempts,
        .run = [&](size_t attempt){
            actions.log(checkpoint_attempt_line(checkpoint, attempt, policy.max_attempts));
            try{
                actions.run(attempt);
                return AttemptResult::PASSED;
            }catch (OperationFailedException& e){
                if (attempt == policy.report_at_failure){
                    actions.report(e);
                }
                if (attempt >= policy.max_attempts){
                    actions.give_up(e);
                    throw;
                }
                return AttemptResult::FAILED;
            }
        },
        .reset = [&](size_t next_attempt){
            actions.log(checkpoint_reset_line(checkpoint, next_attempt));
            actions.reset();
            stats.m_reset++;
            actions.update_stats();
            return true;
        },
        .log = actions.log,
    };
    return run_checkpoint_attempts(checkpoint, attempts).attempts;
}


void record_checkpoint_saved(
    const CheckpointStep& update_stats,
    const std::function<void(std::string_view message)>& notify,
    AutoStoryStats& stats
){
    stats.m_checkpoint++;
    update_stats();
    notify(CHECKPOINT_SAVED_MESSAGE);
}



} // namespace Pokemon
} // namespace PokemonAutomation
