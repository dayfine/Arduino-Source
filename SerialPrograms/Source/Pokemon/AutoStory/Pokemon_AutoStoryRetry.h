/*  AutoStory Retry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStoryRetry_H
#define PokemonAutomation_Pokemon_AutoStoryRetry_H

#include <functional>
#include <string>
#include "CommonFramework/Exceptions/OperationFailedException.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The game steps around an AutoStory checkpoint attempt. Each game supplies its
//  own; tests supply a scripted one.
class CheckpointRetryHooks{
public:
    virtual ~CheckpointRetryHooks() = default;

    virtual void log(const std::string& message) = 0;

    //  Wait for the controller commands sent so far to finish.
    virtual void wait_for_all_requests() = 0;

    //  Save the game before the first attempt. A save that throws counts as a
    //  failed attempt, and later attempts do not save again.
    virtual void save_checkpoint() = 0;

    //  Before each attempt, after the save.
    virtual void before_attempt(size_t attempt) = 0;

    //  After the checkpoint's action returns. Throwing makes the attempt fail.
    virtual void after_success() = 0;

    //  Report a checkpoint that keeps failing, without stopping.
    virtual void report_recoverable(const std::string& message) = 0;

    //  Give up on the checkpoint. Must throw.
    [[noreturn]] virtual void fail(const OperationFailedException& e) = 0;

    //  Reload the last save for the next attempt.
    virtual void reset_for_retry() = 0;
};

struct CheckpointRetryPolicy{
    //  Give up once the attempt number is greater than this.
    size_t max_attempts = 100;
    //  Report once, when this attempt fails.
    size_t report_at_attempt = 10;
};

//  Save, then run `action` until it returns, reloading the save after each attempt
//  that throws OperationFailedException. Other exceptions are not caught.
void run_checkpoint_with_retries(
    CheckpointRetryHooks& hooks,
    const CheckpointRetryPolicy& policy,
    const std::string& checkpoint_text,
    const std::function<void(size_t attempt)>& action
);


}
}
#endif
