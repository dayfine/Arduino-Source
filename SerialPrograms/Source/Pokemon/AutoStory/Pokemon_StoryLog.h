/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The log lines an AutoStory program writes around its checkpoints, in
 *  one format for every game: the story engine (Pokemon_StoryEvents.h,
 *  Pokemon_StorySegments.h) writes them, and a game with its own checkpoint
 *  loop (SV's) writes the same lines, so the lab watcher and whoever reads
 *  a log find each checkpoint's start, attempts, resets and pass the same
 *  way. Also the program's stats counts. Nothing here runs a story.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryLog_H
#define PokemonAutomation_Pokemon_StoryLog_H

#include <cstddef>
#include <string>
#include <string_view>
#include "Pokemon_AutoStoryStats.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One event's line:
//      [Event] 00.04/3 talk_mom: start
//      [Event] 00.04/3 talk_mom: failed: expect_failed: has_parcel
//  An event in a section of the checkpoint names it before the event:
//      [Event] 00.28/12 [b1f_east] winston_line: start
std::string event_log_line(
    std::string_view checkpoint, size_t number, std::string_view event, std::string_view status,
    std::string_view section = {}
);

//  A checkpoint starts:
//      [Checkpoint] 00.04: start
std::string checkpoint_start_line(std::string_view checkpoint);
//  The story engine starts a checkpoint of `events` events:
//      [Checkpoint] 00.04: start, 3 events
std::string checkpoint_start_line(std::string_view checkpoint, size_t events);

//  An attempt at a checkpoint starts, counting from 1:
//      [Checkpoint] 00.04: attempt 2
std::string checkpoint_attempt_line(std::string_view checkpoint, size_t attempt);
//  The same, out of the program's limit of `attempts`:
//      [Checkpoint] 00.04: attempt 2/3
std::string checkpoint_attempt_line(std::string_view checkpoint, size_t attempt, size_t attempts);

//  A checkpoint passed on attempt `attempt`, after `in_place` recoveries in
//  place:
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Checkpoint] 00.04: passed on attempt 3 (2 resets), 1 in place
std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt, size_t in_place = 0);

//  An attempt failed and the game resets to the checkpoint's save for
//  attempt `next_attempt`:
//      [Checkpoint] 00.04: failed, resetting for attempt 2
std::string checkpoint_reset_line(std::string_view checkpoint, size_t next_attempt);
//  The story engine reloads within segment `segment`, out of `attempts`:
//      [Segment] 00: 00.02 failed, reloading for attempt 2/3
std::string checkpoint_reset_line(std::string_view segment, std::string_view checkpoint, size_t next_attempt, size_t attempts);


//  What a story run adds to the program's stats.
struct StoryRunCounts{
    size_t segments = 0;
    //  Checkpoints passed (and saved at).
    size_t checkpoints = 0;
    //  Game resets or reloads after failed attempts.
    size_t resets = 0;
};

//  Adds `counts` to the shared AutoStory stats.
void record_story_counts(AutoStoryStats& stats, const StoryRunCounts& counts);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
