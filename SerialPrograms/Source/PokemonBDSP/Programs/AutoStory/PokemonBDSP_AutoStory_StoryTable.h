/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story from a new game to Floaroma, 00.00 to 01.11, loaded from the
 *  embedded S10/S11 authored plans. Under E9, each checkpoint saves at its
 *  end for the next checkpoint's start (checked against the next row's
 *  saves_in map).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTable_H

#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryRngHandoff.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"

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

//  The handoffs a run stops for or runs in-program (AutoStory's options).
struct StoryHandoffs{
    Pokemon::RngHandoffMode starter_rng = Pokemon::RngHandoffMode::OFF;
};

//  What a run from table row `first` to `last` does.
struct StoryRunPlan{
    std::vector<StoryCheckpoint> rows = {};
    StoryHandoff stop = StoryHandoff::NONE;
    //  When stopping for STARTER_RNG, the save-card map name checked at the
    //  end of 00.01 (00.02's saves_in = "Verity Lakefront").
    std::string handoff_saves_in = {};
    //  Rows whose failure ends the program without reloading: a failed 00.02
    //  with Run Starter RNG, whose reset loop is the retry.
    std::set<std::string> fatal_rows = {};
};

//  The rows of `table` from `first` to `last`, both inclusive, as a
//  run with `handoffs` takes them: a run that reaches 00.01 with
//  `handoffs.starter_rng` STOP saves at the Verity Lakefront at the end of
//  00.01 (via E9 end-save policy) and stops there for Starter RNG.
StoryRunPlan plan_story_run(const std::vector<StoryCheckpoint>& table, size_t first, size_t last, const StoryHandoffs& handoffs = {});



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
