/*  Story Save Mode
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When an AutoStory run may save the game. A run's own verdict can be
 *  wrong (FRLG 00.31, 2026-11-10: a warp leg confirmed B3F while still on
 *  B4F), and a save made then overwrites the only good one. So a segment
 *  under test never saves, a verified one saves once at its end, and only
 *  a playthrough of verified segments saves at each checkpoint.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySaveMode_H
#define PokemonAutomation_Pokemon_StorySaveMode_H

#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  When a run saves. A failed run never saves, in any mode.
enum class StorySaveMode{
    //  Testing: never save; the save the run started from stays.
    NONE,
    //  Verification: save once, at the end of a run that passed.
    AFTER_VERIFICATION,
    //  Reproduction (a playthrough of verified segments): save at the end
    //  of each checkpoint that passed.
    PER_CHECKPOINT,
};

// Returns the wire and log name of a save mode: "none", "after_verification", "per_checkpoint".
std::string story_save_mode_name(StorySaveMode mode);

//  How a checkpoint ended: passed (and its postconditions, where checked,
//  did not fail) or not.
enum class CheckpointVerdict{
    PASSED,
    FAILED,
};

//  Where a save would be made.
enum class SavePoint{
    //  A checkpoint's end, with more checkpoints of the run after it.
    CHECKPOINT_END,
    //  The last checkpoint's end: the run's end state.
    RUN_END,
};

//  Whether a run in `mode` saves at `point` after a checkpoint that ended
//  as `verdict`.
bool story_save_allowed(StorySaveMode mode, CheckpointVerdict verdict, SavePoint point);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
