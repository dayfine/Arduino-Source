/*  BDSP AutoStory - Table Routes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
#include "Data/PokemonBDSP_AutoStoryRobustRoute.h"
#include "Data/PokemonBDSP_AutoStoryWallRecovery.h"
#include "PokemonBDSP_AutoStoryCheckpoint.h"
#include "PokemonBDSP_AutoStoryPlaces.h"
#include "Utils/PokemonBDSP_AutoStoryRouteChoice.h"
#include "PokemonBDSP_AutoStoryTable.h"
#include "PokemonBDSP_AutoStoryWalkSteps.h"
#include "PokemonBDSP_AutoStoryTableRoutes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::GridPose;
using Pokemon::TilePos;


namespace{

std::optional<GridPose> pose_after_step(const std::optional<GridPose>& at, const BdspStep& step){
    if (step.action == "route"){
        return std::get<RouteStepArgs>(step.args).target;
    }
    if (step.pose_after){
        return *step.pose_after;
    }
    return step.action == "legs" || step.action == "step" ? std::nullopt : at;
}

//  A route step after a "measured" legs step plans from the one tile the
//  taps measured: one TableRoute for each tile of `at`.
void add_measured_routes(std::vector<TableRoute>& routes, const std::string& row, const BdspStep& step, const GridPose& at){
    const GridPose& target = std::get<RouteStepArgs>(step.args).target;
    for (const TilePos& pos : at.tiles){
        const GridPose one{.map = at.map, .tiles = {pos}, .facing = at.facing};
        routes.push_back({row, &step, one, story_plan_world(at.map, target.map)});
    }
}

} // namespace


std::vector<TableRoute> table_routes(){
    std::vector<TableRoute> routes;
    for (const StoryCheckpoint& row : story_checkpoints()){
        std::optional<GridPose> at = row.start;
        bool measured = false;
        for (const BdspStep& step : row.steps){
            const bool known = at && !at->tiles.empty();
            if (step.action == "route" && known && std::exchange(measured, false)){
                add_measured_routes(routes, row.id, step, *at);
            }else if (step.action == "route"){
                const GridPose& target = std::get<RouteStepArgs>(step.args).target;
                routes.push_back({row.id, &step, at, known ? story_plan_world(at->map, target.map) : nullptr});
            }
            measured = false;
            if (step.action == "legs" && std::get<LegsStepArgs>(step.args).variant == "measured" && known && !step.pose_after){
                at = measured_ends(*at, step);
                measured = at.has_value();
                continue;
            }
            at = pose_after_step(at, step);
        }
    }
    return routes;
}

std::vector<TilePos> world_tiles(const StoryWorld& world, const GridPose& pose){
    std::vector<TilePos> tiles;
    for (const TilePos& pos : pose.tiles){
        tiles.push_back(story_to_world(world, pose.map, pos));
    }
    return tiles;
}

RouteStepPlan plan_table_route(const TableRoute& route){
    const StoryWorld& world = *route.world;
    const RouteStepArgs& args = std::get<RouteStepArgs>(route.step->args);
    const std::vector<TilePos> targets = world_tiles(world, args.target);
    const auto goal = [&](const TilePos& pos){ return std::find(targets.begin(), targets.end(), pos) != targets.end(); };
    RouteStepPlan plan{.start = world_tiles(world, *route.from), .legs = {}};
    const MoveSpeed speed = route.row > std::string(RUNNING_SHOES_ROW) ? MoveSpeed::RUN : MoveSpeed::WALK;
    plan.legs = plan_fastest_route({
        .world = world, .from = plan.start, .goal = goal, .costs = *story_route_costs(args.variant), .speed = speed,
        .goal_is_trigger = args.variant == ROUTE_ONTO_TRIGGER,
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
