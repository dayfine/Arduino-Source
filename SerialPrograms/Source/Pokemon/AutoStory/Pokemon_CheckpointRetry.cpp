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
    for (size_t attempt = 1;; attempt++){
        try{
            actions.log(checkpoint_attempt_line(checkpoint, attempt, policy.max_attempts));
            actions.run(attempt);
            actions.log(checkpoint_pass_line(checkpoint, attempt));
            return attempt;
        }catch (OperationFailedException& e){
            if (attempt == policy.report_at_failure){
                actions.report(e);
            }
            if (attempt >= policy.max_attempts){
                actions.give_up(e);
                throw;
            }
            actions.log(checkpoint_reset_line(checkpoint, attempt + 1));
            actions.reset();
            stats.m_reset++;
            actions.update_stats();
        }
    }
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
