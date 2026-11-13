/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story from a new game to Floaroma, 00.00 to 01.11, as rows of the
 *  shared story steps (Pokemon/AutoStory/Pokemon_StorySteps.h): plain
 *  data, as FRLG's table is, so the tests plan its routes and run it
 *  against a fake game. Each row but 00.00 saves at its start, which is
 *  where the row before it ends, and only if the save screen's card names
 *  the row's map (saves_in).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H

#include <string_view>
#include <vector>
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Checkpoints 00.00 to 01.11, in order: segment 00 (the intro to the
//  Poketch) and segment 01 (Jubilife to Floaroma).
const std::vector<StoryCheckpoint>& story_checkpoints();

//  Mom gives the Running Shoes in this row's step: from then on, B held
//  with the D-pad runs (r24, r27).
inline constexpr std::string_view RUNNING_SHOES_ROW = "00.03";
//  Story step name in which Mom gives the Running Shoes.
inline constexpr std::string_view RUNNING_SHOES_STEP = "rowan_and_dawn";

//  00.01 ends at the Verity Lakefront (17, 14) facing north, where 00.02 and
//  Starter RNG (Start point: Verity Lakefront) start.
inline constexpr std::string_view STARTER_RNG_HANDOFF_ROW = "00.01";

//  An upstream RNG program the user may run part-way through the story.
enum class StoryHandoff{
    NONE,
    //  The run stops after 00.01 saves at the Verity Lakefront (17, 14),
    //  facing Lake Verity's entrance, where Starter RNG starts.
    STARTER_RNG,
};

//  The handoffs a run stops for (AutoStory's options).
struct StoryHandoffs{
    bool starter_rng = false;
};

//  What a run from table row `first` to `last` does.
struct StoryRunPlan{
    //  The checkpoints to run, in order. Copies, not pointers: a row the plan
    //  changes (the handoff save) must not change the table, and the run's
    //  steps capture these rows, which outlive the call.
    std::vector<StoryCheckpoint> rows;
    //  NONE: the run ends after `rows`. STARTER_RNG: the run stops after
    //  `rows`, the last of which (00.01) saves at the Verity Lakefront.
    StoryHandoff stop = StoryHandoff::NONE;
};

//  The rows of `table` from `first` to `last`, both inclusive, as a
//  run with `handoffs` takes them: a run that reaches 00.01 with
//  `handoffs.starter_rng` on saves at the Verity Lakefront at the end of
//  00.01 and stops there for Starter RNG.
StoryRunPlan plan_story_run(const std::vector<StoryCheckpoint>& table, size_t first, size_t last, const StoryHandoffs& handoffs = {});



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
