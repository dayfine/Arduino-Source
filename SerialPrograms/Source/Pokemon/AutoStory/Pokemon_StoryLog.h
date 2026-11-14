/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The log lines an AutoStory program writes around its checkpoints, in
 *  one format for every game: the story engine (Pokemon_StoryEvents.h,
 *  Pokemon_StorySegments.h) writes them, and SV's AutoStory writes the same
 *  checkpoint start, attempt, reset and pass lines, so the program developer
 *  and user find each checkpoint's start, attempts, resets and pass the same
 *  way in any game's log. Nothing here runs a story.
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

//  The [Checkpoint] lines below name a checkpoint by its id. A game whose
//  checkpoints have their own text that already says it is a checkpoint
//  (SV's "Checkpoint:: 004 - 02 ...") passes that as `display_text`, and
//  the line starts with it in place of "[Checkpoint] <id>":
//      Checkpoint:: 004 - 02 ...: start

//  A checkpoint starts; the story engine also says it has `events` events:
//      [Checkpoint] 00.04: start
//      [Checkpoint] 00.04: start, 3 events
std::string checkpoint_start_line(
    std::string_view checkpoint, std::optional<size_t> events = std::nullopt, std::string_view display_text = ""
);

//  An attempt at a checkpoint starts, counting from 1, out of the
//  program's limit of `attempts` if it has one:
//      [Checkpoint] 00.04: attempt 2
//      [Checkpoint] 00.04: attempt 2/3
std::string checkpoint_attempt_line(
    std::string_view checkpoint, size_t attempt, std::optional<size_t> attempts = std::nullopt,
    std::string_view display_text = ""
);

//  A checkpoint passed on attempt `attempt`, counting from 1; every attempt
//  before it ended in a reset:
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Checkpoint] 00.04: passed on attempt 3 (2 resets)
std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt, std::string_view display_text = "");

//  An attempt failed and the game resets to the checkpoint's save for
//  attempt `next_attempt`:
//      [Checkpoint] 00.04: failed, resetting for attempt 2
std::string checkpoint_reset_line(std::string_view checkpoint, size_t next_attempt, std::string_view display_text = "");

//  The story engine reloads within segment `segment`, out of `attempts`:
//      [Segment] 00: 00.02 failed, reloading for attempt 2/3
std::string segment_reload_line(std::string_view segment, std::string_view checkpoint, size_t next_attempt, size_t attempts);

//  The full lines above go to the log. The video overlay, which wraps its
//  lines, shows each one short, without its leading "[Tag] ":
//      [Event] 12.03/3 fly_pallet: start  ->  12.03/3 fly_pallet: start
//      [Checkpoint] 12.03: start, 18 events  ->  12.03: start, 18 events
//  A line without a tag, like SV's display text, is returned unchanged.
std::string story_short_line(std::string_view line);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
