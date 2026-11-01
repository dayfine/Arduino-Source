/*  Story Segments
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story segment as a table of checkpoints. Each row names a checkpoint
 *  and holds a factory that builds it, so every attempt runs a checkpoint
 *  built fresh, with nothing the last attempt used up. The runner takes the
 *  checkpoints in order from a save; a checkpoint that fails is answered
 *  the one way the event model has no recovery for: the game is reloaded
 *  to its last save, the checkpoint is rebuilt and run again, up to the
 *  policy's attempts. Then the segment stops and the report names the
 *  checkpoint, the event and the reason.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySegments_H
#define PokemonAutomation_Pokemon_StorySegments_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One row of a segment. `build` makes the checkpoint anew for each attempt.
//  `saves` has the segment save the game (SegmentHooks::save) after the
//  checkpoint's end check, when the checkpoint has no save of its own.
struct CheckpointSpec{
    std::string id;
    std::string title;
    bool saves = false;
    MoveOnlyFunction<Checkpoint()> build;
};

struct Segment{
    std::string id;
    std::vector<CheckpointSpec> checkpoints;
};


//  What the game lends the runner: how to save, how to reload the last save
//  (the run state is emptied first; the reload's Outcome says where the
//  player stands), and where to log.
struct SegmentHooks{
    Act save;
    Act reload;
    EventLogger log;
};

struct SegmentPolicy{
    //  Runs of one checkpoint, reloading between them, before the segment stops.
    size_t attempts_per_checkpoint = 3;
};

//  The checkpoints to run, by id, both inclusive. An unset end is the
//  segment's first or last checkpoint.
struct SegmentRange{
    std::optional<std::string> from;
    std::optional<std::string> to;
};


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
};

//  Runs the checkpoints in `range`, in order. Logs through `hooks.log`,
//  around the checkpoint lines:
//      [Segment] 00: start, 3 checkpoints
//      [Segment] 00: 00.02 failed, reloading for attempt 2/3
//      [Segment] 00: done, 3 checkpoints
//      [Segment] 00: failed at 00.02 talk_mom: expect_failed: dialogue opened
SegmentReport run_segment(
    RunState& state, Segment& segment, const SegmentRange& range,
    const SegmentPolicy& policy, SegmentHooks& hooks
);

//  A readable dump for review: the rows, then each checkpoint built once
//  and described as describe(const Checkpoint&) does. Nothing runs.
std::string describe(Segment& segment);



}
}
#endif
