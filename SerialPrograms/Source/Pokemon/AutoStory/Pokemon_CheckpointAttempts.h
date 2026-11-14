/*  Checkpoint Attempts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The retry loop every AutoStory runs a checkpoint in: run an attempt, and
 *  when it fails go back to the checkpoint's start save and try again,
 *  until an attempt passes, fails in a way another attempt would repeat,
 *  the last attempt fails, or a reset fails. SV's loop
 *  (Pokemon_CheckpointRetry.h), the story segments and the story engine all
 *  run their checkpoints through it, so each writes the same pass line
 *  (Pokemon_StoryLog.h) and counts attempts the same way. The game supplies
 *  the attempt and the reset.
 *
 */

#ifndef PokemonAutomation_Pokemon_CheckpointAttempts_H
#define PokemonAutomation_Pokemon_CheckpointAttempts_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  How one attempt at a checkpoint ended.
enum class AttemptResult{
    PASSED,
    //  Failed; another attempt from the start save may pass.
    FAILED,
    //  Failed in a way another attempt would repeat (the game is not
    //  running): no more attempts.
    FINAL,
};

//  Writes one log line.
using CheckpointLogger = std::function<void(const std::string& line)>;

//  What the game does at each step of the retry loop.
struct CheckpointAttempts{
    //  Attempts before the loop gives up.
    size_t max_attempts;
    //  Runs attempt `attempt`, counting from 1.
    std::function<AttemptResult(size_t attempt)> run;
    //  Goes back to the checkpoint's start save before attempt
    //  `next_attempt`, writing its own log line. Returns false when it could
    //  not, which ends the loop.
    std::function<bool(size_t next_attempt)> reset;
    CheckpointLogger log;
    //  False when `checkpoint` is the game's own text that already says it
    //  is a checkpoint, so the pass line leaves out "[Checkpoint] "
    //  (checkpoint_start_line()).
    bool add_checkpoint_prefix = true;
};

//  What the loop came to.
struct CheckpointAttemptsResult{
    bool passed = false;
    //  Attempts run, counting the last one.
    size_t attempts = 0;
};

//  Runs `checkpoint` until an attempt passes, an attempt is FINAL,
//  `max_attempts` attempts failed, or a reset failed. Writes the pass line.
//  Exceptions from `run` and `reset` pass through.
CheckpointAttemptsResult run_checkpoint_attempts(std::string_view checkpoint, const CheckpointAttempts& attempts);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
