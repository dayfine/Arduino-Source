/*  Story Engine Report
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What an engine run of one checkpoint reports.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEnginePolicy_H
#define PokemonAutomation_Pokemon_StoryEnginePolicy_H

#include <optional>
#include "Pokemon/AutoStory/Pokemon_StoryCheckpointTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How a checkpoint ended: passed (and its postconditions, where checked,
//  did not fail) or not.
enum class CheckpointVerdict{
    PASSED,
    FAILED,
};

//  An engine run of one checkpoint.
struct StoryEngineReport{
    //  The steps' run. A checkpoint that failed its lint has none.
    CheckpointReport run = {};
    //  PASSED when the steps completed and no postcondition failed.
    CheckpointVerdict verdict = CheckpointVerdict::FAILED;
    //  Checked after steps that completed.
    std::optional<PostconditionReport> postconditions = std::nullopt;
    //  The game saved at the checkpoint's end.
    bool saved = false;
    //  A SAVE step among its steps saved: a reload now returns there, not
    //  to the save the checkpoint started from.
    bool saved_part_way = false;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
