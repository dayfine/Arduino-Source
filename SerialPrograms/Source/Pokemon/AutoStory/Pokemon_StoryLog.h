/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The checkpoint log lines every game's AutoStory writes.
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
//  "Checkpoint:: 004 - 02 ...") passes that text as `checkpoint` with
//  AS_GIVEN, and the line starts with the text alone:
//      Checkpoint:: 004 - 02 ...: start
enum class CheckpointPrefix{
    ADD,
    AS_GIVEN,
};

//  A checkpoint starts; the story engine also says it has `events` events:
//      [Checkpoint] 00.04: start
//      [Checkpoint] 00.04: start, 3 events
std::string checkpoint_start_line(
    std::string_view checkpoint,
    std::optional<size_t> events = std::nullopt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  An attempt at a checkpoint starts, counting from 1, out of the
//  program's limit of `attempts` if it has one:
//      [Checkpoint] 00.04: attempt 2
//      [Checkpoint] 00.04: attempt 2/3
std::string checkpoint_attempt_line(
    std::string_view checkpoint,
    size_t attempt,
    std::optional<size_t> attempts = std::nullopt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  A checkpoint passed on attempt `attempt`, counting from 1; every attempt
//  before it ended in a reset:
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Checkpoint] 00.04: passed on attempt 3 (2 resets)
std::string checkpoint_pass_line(
    std::string_view checkpoint,
    size_t attempt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  An attempt failed and the game resets to the checkpoint's save for
//  attempt `next_attempt`, out of the limit of `attempts` if it has one:
//      [Checkpoint] 00.04: failed, resetting for attempt 2
//      [Checkpoint] 00.04: failed, resetting for attempt 2/3
std::string checkpoint_reset_line(
    std::string_view checkpoint,
    size_t next_attempt,
    std::optional<size_t> attempts = std::nullopt,
    CheckpointPrefix prefix_kind = CheckpointPrefix::ADD
);

//  `line` without its leading "[Tag] ", for the video overlay; a line with
//  no tag is returned unchanged.
std::string story_short_line(std::string_view line);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
