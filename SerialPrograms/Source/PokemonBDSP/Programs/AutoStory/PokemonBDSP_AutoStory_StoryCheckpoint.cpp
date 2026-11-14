/*  BDSP AutoStory - Story checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::RunState;
using Pokemon::StoryStep;


std::optional<RobustPlanCosts> story_route_costs(const std::string& action){
    if (action.empty() || action == ROUTE_ONTO_TRIGGER || action == ROUTE_INTO_SCENE || action == "route"){
        return route_plan_costs();
    }
    if (action == "town"){
        return town_plan_costs();
    }
    if (action == "robust"){
        return RobustPlanCosts();
    }
    if (action == "line"){
        return waypoint_plan_costs();
    }
    return std::nullopt;
}


Pokemon::Segment make_story_segment(
    const std::vector<StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor,
    std::set<std::string>& saved
){
    static const StoryStep SAVE_AT_START = []{
        StoryStep step;
        step.kind = Pokemon::StoryStepKind::SAVE;
        step.name = "save";
        step.why = "the save a failed step resets to";
        return step;
    }();
    Pokemon::Segment segment;
    segment.id = "BDSP";
    for (const StoryCheckpoint& row : rows){
        segment.checkpoints.push_back(Pokemon::CheckpointSpec{
            .id = row.id, .title = row.title, .saves = false,
            .build = [&row, &executor, &saved]{
                Pokemon::Checkpoint steps = Pokemon::build_story_checkpoint(row, executor);
                if (row.on_failure != StoryFailure::RELOAD_AND_RETRY){
                    return steps;
                }
                StoryStep save_at_start = SAVE_AT_START;
                save_at_start.args.emplace(SAVES_IN_ARG, row.saves_in);
                Pokemon::Event save = Pokemon::story_event(save_at_start, executor);
                save.skip_if.push_back(Pokemon::condition("saved at the start", [&saved, id = row.id](RunState&){
                    return saved.contains(id);
                }));
                //  A save that fails is a failed first attempt: the retry
                //  resets and does not save again.
                save.act = [&executor, &saved, id = row.id, save_at_start](RunState& state){
                    saved.insert(id);
                    return executor.run(save_at_start, state);
                };
                Pokemon::Checkpoint checkpoint;
                checkpoint.id = row.id;
                checkpoint.events.push_back(std::move(save));
                for (Pokemon::Event& event : steps.events){
                    checkpoint.events.push_back(std::move(event));
                }
                return checkpoint;
            },
        });
    }
    return segment;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
