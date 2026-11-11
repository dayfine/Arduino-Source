/*  BDSP AutoStory - Table Routes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
#include "Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "Utils/PokemonBDSP_AutoStory_RouteChoice.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_TableRoutes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::StoryStepKind;
using Pokemon::TilePos;


namespace{

using Pokemon::story_arg;

std::optional<Pokemon::Pose> pose_after_step(const std::optional<Pokemon::Pose>& at, const Pokemon::StoryStep& step){
    if (step.kind == StoryStepKind::ROUTE){
        return Pokemon::pose_of(story_arg<Pokemon::GridPose>(step.args, "target"));
    }
    if (step.pose_after){
        return Pokemon::pose_of(*step.pose_after);
    }
    return step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::STEP ? std::nullopt : at;
}

//  A ROUTE step after a "measured" LEGS step plans from the one tile the
//  taps measured: one TableRoute for each tile of `at`.
void add_measured_routes(std::vector<TableRoute>& routes, const std::string& row, const Pokemon::StoryStep& step, const Pokemon::Pose& at){
    const Pokemon::Pose target = Pokemon::pose_of(story_arg<Pokemon::GridPose>(step.args, "target"));
    for (const TilePos& pos : Pokemon::tiles_of(at)){
        const Pokemon::Pose one = Pokemon::tile_pose(at.map, {pos}, Pokemon::facing_of(at));
        routes.push_back({row, &step, one, story_plan_world(at.map, target.map)});
    }
}

} // namespace


std::vector<TableRoute> table_routes(){
    std::vector<TableRoute> routes;
    for (const StoryCheckpoint& row : story_checkpoints()){
        std::optional<Pokemon::Pose> at = row.start ? std::optional<Pokemon::Pose>(Pokemon::pose_of(*row.start)) : std::nullopt;
        bool measured = false;
        for (const Pokemon::StoryStep& step : row.steps){
            const bool known = at && !Pokemon::tiles_of(*at).empty();
            if (step.kind == StoryStepKind::ROUTE && known && std::exchange(measured, false)){
                add_measured_routes(routes, row.id, step, *at);
            }else if (step.kind == StoryStepKind::ROUTE){
                const Pokemon::Pose target = Pokemon::pose_of(story_arg<Pokemon::GridPose>(step.args, "target"));
                routes.push_back({row.id, &step, at, known ? story_plan_world(at->map, target.map) : nullptr});
            }
            measured = false;
            if (step.kind == StoryStepKind::LEGS && step.action == "measured" && known && !step.pose_after){
                at = measured_ends(*at, step);
                measured = at.has_value();
                continue;
            }
            at = pose_after_step(at, step);
        }
    }
    return routes;
}

std::vector<TilePos> world_tiles(const StoryWorld& world, const Pokemon::Pose& pose){
    std::vector<TilePos> tiles;
    for (const TilePos& pos : Pokemon::tiles_of(pose)){
        tiles.push_back(story_to_world(world, pose.map, pos));
    }
    return tiles;
}

RouteStepPlan plan_table_route(const TableRoute& route){
    const StoryWorld& world = *route.world;
    const Pokemon::Pose target = Pokemon::pose_of(story_arg<Pokemon::GridPose>(route.step->args, "target"));
    const std::vector<TilePos> targets = world_tiles(world, target);
    const auto goal = [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); };
    RouteStepPlan plan{.start = world_tiles(world, *route.from)};
    const MoveSpeed speed = route.row > std::string(RUNNING_SHOES_ROW) ? MoveSpeed::RUN : MoveSpeed::WALK;
    plan.legs = plan_fastest_route({
        .world = world, .from = plan.start, .goal = goal, .costs = *story_route_costs(route.step->action), .speed = speed,
        .goal_is_trigger = route.step->action == ROUTE_ONTO_TRIGGER,
    });
    if (!plan.legs){
        plan.legs = plan_recovered_route(world, plan.start, goal, RobustPlanCosts());
        plan.default_costs = plan.legs.has_value();
    }
    return plan;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
