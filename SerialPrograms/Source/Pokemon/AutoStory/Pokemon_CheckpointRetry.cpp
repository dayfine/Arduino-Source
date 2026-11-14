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
    std::string_view checkpoint_text,
    const CheckpointRetryPolicy& policy,
    const CheckpointRetryActions& actions,
    AutoStoryStats& stats
){
    actions.log(checkpoint_start_line(checkpoint_text, std::nullopt, checkpoint_text));
    const CheckpointAttempts attempts{
        .max_attempts = policy.max_attempts,
        .run = [&](size_t attempt){
            actions.log(checkpoint_attempt_line(checkpoint_text, attempt, policy.max_attempts, checkpoint_text));
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
            actions.log(checkpoint_reset_line(checkpoint_text, next_attempt, checkpoint_text));
            actions.reset();
            stats.m_reset++;
            actions.update_stats();
            return true;
        },
        .log = actions.log,
        .display_text = checkpoint_text,
    };
    return run_checkpoint_attempts(checkpoint_text, attempts).attempts;
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
