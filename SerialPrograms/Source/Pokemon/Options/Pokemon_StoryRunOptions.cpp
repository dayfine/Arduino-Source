/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryRunOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


StoryRunOptions::StoryRunOptions()
    : GroupOption("Checkpoint Retries", LockMode::LOCK_WHILE_RUNNING)
    , ATTEMPTS_PER_CHECKPOINT(
        "<b>Attempts per checkpoint:</b><br>A failed checkpoint reloads its save and runs again, up to this many times in all.",
        LockMode::LOCK_WHILE_RUNNING,
        3, 1, 20
    )
    , STOP_AT_FIRST_FAILURE(
        "<b>Stop at the first failure:</b><br>Stop without reloading, leaving the game where it failed for a look.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
{
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(STOP_AT_FIRST_FAILURE);
}

SegmentPolicy StoryRunOptions::policy() const{
    SegmentPolicy policy;
    policy.attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT;
    policy.stop_at_first_failure = STOP_AT_FIRST_FAILURE;
    return policy;
}



}
}
