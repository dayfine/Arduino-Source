/*  Checkpoint Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryLog.h"
#include "Pokemon_CheckpointAttempts.h"

namespace PokemonAutomation{
namespace Pokemon{


CheckpointAttemptsResult run_checkpoint_attempts(std::string_view checkpoint, const CheckpointAttempts& attempts){
    CheckpointAttemptsResult result;
    for (size_t attempt = 1;; attempt++){
        result.attempts = attempt;
        const AttemptResult outcome = attempts.run(attempt);
        if (outcome == AttemptResult::PASSED){
            result.passed = true;
            attempts.log(checkpoint_pass_line(checkpoint, attempt, attempts.display_text));
            return result;
        }
        if (outcome == AttemptResult::FINAL || attempt >= attempts.max_attempts){
            return result;
        }
        if (!attempts.reset(attempt + 1)){
            return result;
        }
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
