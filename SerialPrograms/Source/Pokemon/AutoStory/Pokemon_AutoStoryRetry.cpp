/*  AutoStory Retry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_AutoStoryRetry.h"

namespace PokemonAutomation{
namespace Pokemon{


void run_checkpoint_with_retries(
    CheckpointRetryHooks& hooks,
    const CheckpointRetryPolicy& policy,
    const std::string& checkpoint_text,
    const std::function<void(size_t attempt)>& action
){
    for (size_t attempt = 0;; attempt++){
        try{
            if (attempt == 0){
                hooks.save_checkpoint();
            }
            hooks.before_attempt(attempt);
            hooks.log(checkpoint_text + " Num attempts: " + std::to_string(attempt));

            hooks.wait_for_all_requests();
            action(attempt);
            hooks.after_success();
            return;
        }catch (OperationFailedException& e){
            if (attempt == policy.report_at_attempt){
                hooks.report_recoverable(std::to_string(attempt) + " failed attempts. " + checkpoint_text);
            }
            if (attempt > policy.max_attempts){
                hooks.fail(e);
            }
            hooks.wait_for_all_requests();
            hooks.log("Resetting from checkpoint.");
            hooks.reset_for_retry();
        }
    }
}


}
}
