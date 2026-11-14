/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story is split at three levels: a segment is a chapter of
 *  checkpoints, a checkpoint is the stretch between two saves and the unit
 *  a run retries, and an event is one step inside a checkpoint. This file
 *  writes the checkpoint lines: start, attempt, reset and pass, in one
 *  format for every game, so the program developer and user find each
 *  checkpoint's attempts the same way in any game's log. Nothing here runs
 *  a story.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryLog_H
#define PokemonAutomation_Pokemon_StoryLog_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  The lines below start with "[Checkpoint] " and then `checkpoint`. A game
//  whose checkpoint text already says it is a checkpoint (SV's
//  "Checkpoint:: 004 - 02 ...") passes that text as `checkpoint` and
//  `add_checkpoint_prefix` false, and the line starts with the text alone:
//      Checkpoint:: 004 - 02 ...: start

//  A checkpoint starts; the story engine also says it has `events` events:
//      [Checkpoint] 00.04: start
//      [Checkpoint] 00.04: start, 3 events
std::string checkpoint_start_line(
    std::string_view checkpoint,
    std::optional<size_t> events = std::nullopt,
    bool add_checkpoint_prefix = true
);

//  An attempt at a checkpoint starts, counting from 1, out of the
//  program's limit of `attempts` if it has one:
//      [Checkpoint] 00.04: attempt 2
//      [Checkpoint] 00.04: attempt 2/3
std::string checkpoint_attempt_line(
    std::string_view checkpoint,
    size_t attempt,
    std::optional<size_t> attempts = std::nullopt,
    bool add_checkpoint_prefix = true
);

//  A checkpoint passed on attempt `attempt`, counting from 1; every attempt
//  before it ended in a reset:
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Checkpoint] 00.04: passed on attempt 3 (2 resets)
std::string checkpoint_pass_line(
    std::string_view checkpoint,
    size_t attempt,
    bool add_checkpoint_prefix = true
);

//  An attempt failed and the game resets to the checkpoint's save for
//  attempt `next_attempt`, out of the limit of `attempts` if it has one:
//      [Checkpoint] 00.04: failed, resetting for attempt 2
//      [Checkpoint] 00.04: failed, resetting for attempt 2/3
std::string checkpoint_reset_line(
    std::string_view checkpoint,
    size_t next_attempt,
    std::optional<size_t> attempts = std::nullopt,
    bool add_checkpoint_prefix = true
);

//  The full lines above go to the log. The video overlay, which wraps its
//  lines, shows each one short, without its leading "[Tag] ":
//      [Checkpoint] 12.03: start, 18 events  ->  12.03: start, 18 events
//  Any line with a leading "[Tag] " is shortened the same way. A line
//  without a tag, like SV's checkpoint text, is returned unchanged.
std::string story_short_line(std::string_view line);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
