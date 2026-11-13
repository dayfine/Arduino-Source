/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story from a new game to Floaroma, 00.00 to 01.11, as rows of the
 *  shared story steps: plain data loaded from the embedded authored plans.
 *
 */

#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  `row` with a save appended at its end, where the next checkpoint starts:
//  in BDSP rows normally save at their start, so a run that stops after
//  00.01 for Starter RNG makes 00.02's Lakefront save before stopping. As
//  every start save does, it checks that the save card names `saves_in`
//  (the next row's), and stops the program if not.
StoryCheckpoint with_handoff_save(StoryCheckpoint row, const std::string& saves_in){
    Pokemon::StoryStep save;
    save.kind = Pokemon::StoryStepKind::SAVE;
    save.name = "save_for_starter_rng";
    save.action = saves_in;
    save.why = "the save at the Verity Lakefront where Starter RNG starts";
    if (!row.steps.empty()){
        const Pokemon::StoryStep& last = row.steps.back();
        if (last.pose_after){
            save.pose_after = last.pose_after;
        }else if (last.kind == Pokemon::StoryStepKind::ROUTE){
            const Pokemon::GridPose& target = Pokemon::story_arg<Pokemon::GridPose>(last.args, "target");
            save.pose_after = Pokemon::TileDeclared{
                .map = target.map, .tiles = target.tiles, .facing = target.facing
            };
        }
    }
    row.steps.push_back(std::move(save));
    return row;
}

} // namespace


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = load_authored_story();
    return checkpoints;
}

StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
){
    StoryRunPlan plan;
    for (size_t c = first; c <= last && c < checkpoints.size(); c++){
        const StoryCheckpoint& row = checkpoints[c];
        if (row.id == STARTER_RNG_HANDOFF_ROW && handoffs.starter_rng){
            const std::string saves_in = c + 1 < checkpoints.size() ? checkpoints[c + 1].saves_in : std::string();
            plan.rows.push_back(with_handoff_save(row, saves_in));
            plan.stop = StoryHandoff::STARTER_RNG;
            return plan;
        }
        plan.rows.push_back(row);
    }
    return plan;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
