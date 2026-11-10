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
#include <string>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StoryLog.h"
#include "Types/Pokemon_StorySegmentTypes.h"

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

// Collection of ordered checkpoints that make up a story segment.
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
    //  Optional: each [Segment], [Checkpoint] and [Event] line again,
    //  without its tag ("00.03/2 below_oaks_trigger: start"), for the video
    //  overlay, so whoever watches the stream sees the step under way; and,
    //  once run records land (#132), for the run's events.
    EventLogger status;
    //  Optional: the game's postcondition check of a checkpoint, by id
    //  (check_postconditions(), Pokemon_StoryPostconditionCheck.h), run
    //  before a save in AFTER_VERIFICATION mode: one that failed stops the
    //  save and fails the run. The game logs its lines.
    MoveOnlyFunction<Outcome(RunState& state, const std::string& checkpoint)> check_postconditions;
};

//  Runs the checkpoints in `range`, in order, saving as `policy.save_mode`
//  allows (story_save_allowed()). Logs through `hooks.log`,
//  around the checkpoint lines:
//      [Segment] 00: start, 3 checkpoints
//      [Segment] 00: 00.02 failed, reloading for attempt 2/3
//      [Checkpoint] 00.02: passed on attempt 2 (1 reset)
//      [Checkpoint] 00.03: passed on attempt 1 (one-shot)
//      [Segment] 00: 2 of 3 checkpoints one-shot
//      [Segment] 00: done, 3 checkpoints
//      [Segment] 00: failed at 00.02 talk_mom: expect_failed: dialogue opened
SegmentReport run_segment(
    RunState& state, Segment& segment, const SegmentRange& range,
    const SegmentPolicy& policy, SegmentHooks& hooks
);


//  Verify mode, for a checkpoint that must be shown to hold up (a battle
//  lost twice or more gets its own checkpoint, saved right before it, and is
//  verified this way): the checkpoint run `runs` times, each from the same
//  save. The checkpoint never saves at its end, so each run after the first
//  reloads the save the first started from.
struct VerifyPolicy{
    size_t runs = 3;
    //  Passing runs the checkpoint needs to be verified.
    size_t passes_needed = 2;
    //  AFTER_VERIFICATION saves once, at the end of the last run, when
    //  every run passed; any other mode never saves.
    StorySaveMode save_mode = StorySaveMode::NONE;
};

// Verification results for multiple runs of a checkpoint.
struct VerifyReport{
    std::string checkpoint;
    size_t runs = 0;
    size_t passes = 0;
    //  At least VerifyPolicy::passes_needed runs passed.
    bool verified = false;
    //  AFTER_VERIFICATION: every run passed and the last saved at its end.
    bool saved = false;
    //  What stopped the runs early: an unknown checkpoint, a bad policy or
    //  a failed reload. Runs that fail are counted, not stopped at.
    Outcome failure;
};

//  Runs checkpoint `id` of `segment` as `policy` says, from the save the
//  game is on. No retries, no in-place recoveries and no save at its end,
//  but for `policy.save_mode` AFTER_VERIFICATION's one save at the end of
//  the last run, when every run passed (after the postcondition check).
//  Logs through `hooks.log`, after each run's checkpoint lines:
//      [Verify] 00.15b: start, 3 runs from one save, 2 must pass
//      [Verify] 00.15b: run 1/3 passed
//      [Verify] 00.15b: run 2/3 failed at rival_battle: battle_lost
//      [Verify] 00.15b: 2/3 passed, 2 needed: verified
VerifyReport verify_checkpoint(
    RunState& state, Segment& segment, const std::string& id,
    const VerifyPolicy& policy, SegmentHooks& hooks
);

//  A readable dump for review: the rows, then each checkpoint built once
//  and described as describe(const Checkpoint&) does. Nothing runs.
std::string describe(Segment& segment);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
