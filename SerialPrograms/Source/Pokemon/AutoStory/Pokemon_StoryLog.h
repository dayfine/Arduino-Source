/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The log lines an AutoStory program writes around its checkpoints, in
 *  one format for every game: the story engine (Pokemon_StoryEvents.h,
 *  Pokemon_StorySegments.h) writes them, and SV's AutoStory writes the same
 *  checkpoint start line, so the program developer and user find each
 *  checkpoint's start, attempts, resets and pass the same way in any game's
 *  log. Nothing here runs a story.
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


//  One event's line:
//      [Event] 00.04/3 talk_mom: start
//      [Event] 00.04/3 talk_mom: failed: expect_failed: has_parcel
std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status);

//  A checkpoint starts; the story engine also says it has `events` events:
//      [Checkpoint] 00.04: start
//      [Checkpoint] 00.04: start, 3 events
std::string checkpoint_start_line(std::string_view checkpoint, std::optional<size_t> events = std::nullopt);

//  An attempt at a checkpoint starts, counting from 1, out of the
//  program's limit of `attempts` if it has one:
//      [Checkpoint] 00.04: attempt 2
//      [Checkpoint] 00.04: attempt 2/3
std::string checkpoint_attempt_line(
    std::string_view checkpoint, size_t attempt, std::optional<size_t> attempts = std::nullopt
);

//  A checkpoint passed on attempt `attempt`, counting from 1; every attempt
//  before it ended in a reset:
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Checkpoint] 00.04: passed on attempt 3 (2 resets)
std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt);

//  An attempt failed and the game resets to the checkpoint's save for
//  attempt `next_attempt`:
//      [Checkpoint] 00.04: failed, resetting for attempt 2
std::string checkpoint_reset_line(std::string_view checkpoint, size_t next_attempt);

//  The story engine reloads within segment `segment`, out of `attempts`:
//      [Segment] 00: 00.02 failed, reloading for attempt 2/3
std::string segment_reload_line(std::string_view segment, std::string_view checkpoint, size_t next_attempt, size_t attempts);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
