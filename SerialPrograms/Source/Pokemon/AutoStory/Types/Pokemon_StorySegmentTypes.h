/*  Story Segment Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The plain data of a segment run (Pokemon_StorySegments.h): its policy,
 *  the range of checkpoints to run and the report of what happened.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySegmentTypes_H
#define PokemonAutomation_Pokemon_StorySegmentTypes_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon_StorySaveMode.h"

namespace PokemonAutomation{
namespace Pokemon{


// Retry and timeout configuration for story segment execution.
struct SegmentPolicy{
    //  Runs of one checkpoint, reloading its save between them, before the
    //  segment stops. 1 stops at the first failure without reloading, leaving
    //  the game where it failed for a look.
    size_t attempts_per_checkpoint = 3;
    //  When the run saves. In the other modes no checkpoint after the first
    //  has a save at its start, so only the first is retried with a reload.
    //  The options' default is NONE; the engine's stays the playthrough's.
    StorySaveMode save_mode = StorySaveMode::PER_CHECKPOINT;
};

//  The checkpoints to run, by id, both inclusive. An unset end is the
//  segment's first or last checkpoint.
struct SegmentRange{
    std::optional<std::string> from;
    std::optional<std::string> to;
};


//  What a story run adds to the program's stats.
struct StoryRunCounts{
    size_t segments = 0;
    //  Checkpoints passed (and saved at).
    size_t checkpoints = 0;
    //  Game resets or reloads after failed attempts.
    size_t resets = 0;
};

//  Adds `counts` to the shared AutoStory stats.
void record_story_counts(const StoryRunCounts& counts, AutoStoryStats& stats);


// Summary of checkpoint attempts and pass status.
struct CheckpointRun{
    std::string id;
    bool passed = false;
    size_t attempts = 0;

    bool one_shot() const;
};

// Execution results and failure diagnostics for a story segment run.
struct SegmentReport{
    bool completed = false;
    //  Both empty when nothing failed; the event is empty when a start
    //  check, an end check, a save or a reload failed.
    std::string failed_checkpoint;
    std::string failed_event;
    Outcome failure;
    size_t checkpoints_run = 0;
    //  Reloads after failed attempts.
    size_t reloads = 0;
    //  Each checkpoint run, in order, the failed one last.
    std::vector<CheckpointRun> runs;

    size_t one_shot_passes() const;
    //  What the run adds to the program's stats (record_story_counts()):
    //  the segment, each checkpoint that passed, each reload.
    StoryRunCounts story_counts() const;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
