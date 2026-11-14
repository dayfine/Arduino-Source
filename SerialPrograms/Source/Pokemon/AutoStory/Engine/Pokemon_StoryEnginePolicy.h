/*  Story Engine Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When an engine run of a checkpoint may save, from the checkpoint's
 *  metadata (autostory/SAVE_POLICY.md on the notes branch): a testing
 *  checkpoint never saves, a verification one saves once after its runs
 *  passed, a reproduction one at its end. A checkpoint whose steps no
 *  longer hash to the ones it was verified with is treated as testing: its
 *  verification was of other steps.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEnginePolicy_H
#define PokemonAutomation_Pokemon_StoryEnginePolicy_H

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_StoryCheckpointTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"

namespace PokemonAutomation{
namespace Pokemon{


//  "sha256:<hex>" of `checkpoint`'s steps JSON as read from its file
//  (StoryCheckpoint::steps_json): any edit to the steps, including a why
//  or a default written out, changes it.
std::string story_steps_hash(const StoryCheckpoint& checkpoint);

//  A run's save mode and the log line that says why.
struct StorySaveDecision{
    StorySaveMode mode = StorySaveMode::NONE;
    std::string reason = {};
};

//  The save mode of a run of `checkpoint`, whose steps hash to `steps_hash`.
StorySaveDecision decide_story_save(const StoryCheckpoint& checkpoint, std::string_view steps_hash);

//  An engine run of one checkpoint.
struct StoryEngineReport{
    //  The steps' run. A checkpoint that failed its lint has none.
    CheckpointReport run = {};
    StorySaveDecision save = {};
    //  PASSED when the steps completed and no postcondition failed.
    CheckpointVerdict verdict = CheckpointVerdict::FAILED;
    //  Checked after steps that completed.
    std::optional<PostconditionReport> postconditions = std::nullopt;
    bool saved = false;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
