/*  Checkpoint Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The retry loop every AutoStory runs a checkpoint in.
 *
 */

#ifndef PokemonAutomation_Pokemon_CheckpointAttempts_H
#define PokemonAutomation_Pokemon_CheckpointAttempts_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include "Pokemon_StoryLog.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How one attempt at a checkpoint ended.
enum class AttemptResult{
    PASSED,
    //  Failed; another attempt from the start save may pass.
    FAILED,
    //  Failed in a way another attempt would repeat: no more attempts.
    FINAL,
};

//  What the game does at each step of the retry loop.
struct CheckpointAttempts{
    //  Attempts before the loop gives up; the first always runs.
    size_t max_attempts = 1;
    //  Runs attempt `attempt`, counting from 1.
    std::function<AttemptResult(size_t attempt)> run = {};
    //  Goes back to the checkpoint's start save before attempt
    //  `next_attempt`; false when it could not, which ends the loop.
    std::function<bool(size_t next_attempt)> reset = {};
    std::function<void(std::string_view line)> log = {};
    //  How the pass line names the checkpoint (CheckpointPrefix).
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD;
};

//  What the loop came to.
struct CheckpointAttemptsResult{
    bool passed = false;
    //  Attempts run, counting the last one.
    size_t attempts = 0;
};

//  Runs `checkpoint` until an attempt passes, an attempt is FINAL,
//  `max_attempts` attempts failed, or a reset failed; logs the pass line.
CheckpointAttemptsResult run_checkpoint_attempts(std::string_view checkpoint, const CheckpointAttempts& attempts);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
