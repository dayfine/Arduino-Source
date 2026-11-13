/*  Story Checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint of a tile game's story table as plain data: where its save
 *  leaves the player and its steps in order. build_story_checkpoint()
 *  (Pokemon_StoryStepExecutor.h) turns it into engine events.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpoint_H
#define PokemonAutomation_Pokemon_StoryCheckpoint_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_GateState.h"
#include "Pokemon_DeclaredPose.h"
#include "Pokemon_StoryCheckpointMetadata.h"
#include "Pokemon_StoryPostconditions.h"
#include "Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


// Milestone row in the story table grouping related steps and starting pose.
struct StoryCheckpoint{
    std::string id;
    std::string title;
    //  Where its starting save leaves the player; none for a new game.
    std::optional<DeclaredPose> start;
    std::vector<StoryStep> steps;
    //  The steps' JSON ("events") as read, dumped compactly; what
    //  story_steps_hash() hashes. Empty for a checkpoint built in code.
    std::string steps_json = {};
    //  What must hold when it is done; checked on its own after a run.
    //  JSON: "metadata": {"postconditions": ...}.
    StoryPostconditions postconditions;
    //  Its phase and verification (JSON "metadata"), which decide when a
    //  run of it may save.
    StoryCheckpointMetadata metadata = {};
    //  Which gates are open and which switches are on at the start, where the
    //  save has any of the story's gates (Silph Co., Pokemon Mansion); none
    //  where the checkpoint does not say, which the run infers from the
    //  story so far, or else observes. JSON: "start_gates".
    std::optional<GateState> start_gates = {};
    //  The run flags that hold at the start, as flags_after would have set
    //  them in the story so far, which a reload loses ("card_key_held" in a
    //  save made after the Card Key). JSON: "start_flags".
    std::vector<std::string> start_flags = {};
};

//  The gate state of `checkpoint` once its first `steps_done` steps are done:
//  its start_gates (nothing open, no switch on if it has none) with the
//  steps' gates_after applied in order. `steps_done` past the last step is
//  the end.
GateState checkpoint_gates(const StoryCheckpoint& checkpoint, size_t steps_done);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
