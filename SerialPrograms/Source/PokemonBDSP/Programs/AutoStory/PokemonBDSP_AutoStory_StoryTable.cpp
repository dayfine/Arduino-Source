/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story from a new game to Floaroma, 00.00 to 01.11, loaded from the
 *  embedded authored plans.
 *
 */

#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  00.02 is the Lake Verity starter and Starly battle checkpoint, which Run
//  Starter RNG in AutoStory replaces with the game event STARTER_RNG_ACTION.
constexpr std::string_view STARTER_RNG_CHECKPOINT_ROW = "00.02";
constexpr std::string_view STARTER_RNG_ACTION = "starter_rng";

//  `row` (00.02) with its Lake Verity steps replaced by the in-program
//  Starter RNG action, keeping 00.02's start pose at VerityLakefront (17, 14)
//  and its end pose at VerityLakefront (17, 14..15).
StoryCheckpoint with_starter_rng_step(StoryCheckpoint row){
    BdspStep step;
    step.name = std::string(STARTER_RNG_ACTION);
    step.action = std::string(STARTER_RNG_ACTION);
    step.why = "Starter RNG at Lake Verity through the Starly battle and Rowan's scene";
    step.args = PlainStepArgs{};
    if (!row.steps.empty()){
        step.pose_after = row.steps.back().pose_after;
    }
    row.steps = {std::move(step)};
    return row;
}

} // namespace


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = load_bdsp_story({}).checkpoints;
    return checkpoints;
}

StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
){
    StoryRunPlan plan;
    for (size_t c = first; c <= last && c < checkpoints.size(); c++){
        const StoryCheckpoint& row = checkpoints[c];
        if (row.id == STARTER_RNG_HANDOFF_ROW && handoffs.starter_rng == Pokemon::RngHandoffMode::STOP){
            plan.rows.push_back(row);
            plan.handoff_saves_in = c + 1 < checkpoints.size() ? checkpoints[c + 1].saves_in : std::string();
            plan.stop = StoryHandoff::STARTER_RNG;
            return plan;
        }
        if (row.id == STARTER_RNG_CHECKPOINT_ROW && handoffs.starter_rng == Pokemon::RngHandoffMode::RUN){
            plan.rows.push_back(with_starter_rng_step(row));
            plan.fatal_rows.insert(row.id);
            continue;
        }
        plan.rows.push_back(row);
    }
    return plan;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
