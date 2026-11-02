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
//  `recover`, if set, is the game's way of recovering in place from a failed
//  event (re-anchor on a landmark, close a stray dialogue) before the event
//  is tried again; see SegmentPolicy::recoveries_per_event.
struct SegmentHooks{
    Act save;
    Act reload;
    EventLogger log;
    MoveOnlyFunction<Outcome(RunState& state, const std::string& event)> recover;
    //  Optional: each [Segment], [Checkpoint] and [Event] line again,
    //  without its tag ("00.03/2 below_oaks_trigger: start"), for the video
    //  overlay, so whoever watches the stream sees the step under way; and,
    //  once run records land (#132), for the run's events.
    EventLogger status;
};

//  How hard a segment tries, for every game: the one place retries are set.
struct SegmentPolicy{
    //  Runs of one checkpoint, reloading its save between them, before the
    //  segment stops.
    size_t attempts_per_checkpoint = 3;
    //  Lab: stop at the first failed attempt without reloading, leaving the
    //  game where it failed for a look.
    bool stop_at_first_failure = false;
    //  Times each event may recover in place (SegmentHooks::recover, then
    //  the event again) once its own recoveries are used up, before its
    //  checkpoint fails and is reloaded. 0 keeps a failure a reload.
    size_t recoveries_per_event = 0;
};

//  The checkpoints to run, by id, both inclusive. An unset end is the
//  segment's first or last checkpoint.
struct SegmentRange{
    std::optional<std::string> from;
    std::optional<std::string> to;
};


//  How one checkpoint went: one attempt with no in-place recovery is a
//  one-shot pass.
struct CheckpointRun{
    std::string id;
    bool passed = false;
    size_t attempts = 0;
    //  In-place recoveries (SegmentPolicy::recoveries_per_event) used, in all attempts.
    size_t recoveries = 0;

    bool one_shot() const;
};

struct SegmentReport{
    bool completed = false;
    //  The run stopped at its first failure (SegmentPolicy::stop_at_first_failure).
    bool stopped_at_first_failure = false;
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
};

//  Runs the checkpoints in `range`, in order. Logs through `hooks.log`,
//  around the checkpoint lines:
//      [Segment] 00: start, 3 checkpoints
//      [Segment] 00: 00.02 failed, reloading for attempt 2/3
//      [Checkpoint] 00.02: passed on attempt 2 (1 reset)
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Segment] 00: 2 of 3 checkpoints one-shot
//      [Segment] 00: done, 3 checkpoints
//      [Segment] 00: 00.02 failed; stopping at the first failure, the game stays where it failed
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
