/*  BDSP AutoStory - Wait beside a wanderer, on the simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP_AutoStory_SimWanderWait.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TilePos;


SimWanderWait::SimWanderWait(GridSim& sim, std::vector<TilePos> still)
    : m_sim(sim)
    , m_still(std::move(still))
{}
bool SimWanderWait::step(Direction direction){
    return m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false).distance > 0;
}
void SimWanderWait::face(Direction direction){
    m_sim.tap(direction, 50ms, false);
}
bool SimWanderWait::ask(){
    const TilePos faced = nearest_tile(m_sim.player().pos).next(m_sim.player().facing);
    bool someone = std::find(m_still.begin(), m_still.end(), faced) != m_still.end();
    for (const SimWanderer& wanderer : detail::sim_wanderers(m_sim)){
        someone |= wanderer.pos == faced;
    }
    const bool talked = m_sim.press_a() && someone;
    m_sim.end_dialogue();
    detail::sim_advance_time(m_sim, 1500ms);
    return talked;
}
void SimWanderWait::wait(Milliseconds time){
    detail::sim_advance_time(m_sim, time);
}


WanderWaitPlan wander_wait_plan(const Pokemon::StoryStep& step){
    const Pokemon::GridPose& stand = Pokemon::story_arg<Pokemon::GridPose>(step.args, "stand");
    const Pokemon::Pose exit = Pokemon::pose_of(*step.pose_after);
    const Direction back = Pokemon::ALL_DIRECTIONS[((size_t)*stand.facing + 2) % 4];
    return WanderWaitPlan{.stands = stand.tiles, .facing = *stand.facing,
        .exit = Pokemon::tiles_of(exit).at(0), .exit_walls = {*Pokemon::facing_of(exit), back}};
}


namespace{

//  The plan's tiles on `world`.
WanderWaitPlan on_world(WanderWaitPlan plan, const StoryWorld& world, const std::string& map){
    for (TilePos& stand : plan.stands){
        stand = story_to_world(world, map, stand);
    }
    plan.exit = story_to_world(world, map, plan.exit);
    return plan;
}

//  Where the people of `map` the simulation does not walk stand: their
//  spawns, on `world`.
std::vector<TilePos> still_people(const GridSim& sim, const StoryWorld& world, const std::string& map){
    std::vector<TilePos> out;
    const std::vector<SimWanderer>& walking = detail::sim_wanderers(sim);
    for (const Pokemon::MapObject& object : story_map(map).objects){
        const bool walks = std::ranges::any_of(walking, [&](const SimWanderer& wanderer){ return wanderer.name == object.name; });
        if (object.kind == Pokemon::ObjectKind::NPC && !walks){
            out.push_back(story_to_world(world, map, {object.x, object.y}));
        }
    }
    return out;
}

} // namespace


Pokemon::Outcome sim_wait_for_wanderer(GridSim& sim, const StoryWorld& world, const Pokemon::Pose& start, const Pokemon::StoryStep& step){
    const TilePos from = story_to_world(world, start.map, Pokemon::tiles_of(start).at(0));
    SimWanderWait body(sim, still_people(sim, world, start.map));
    const WanderWaitResult result = wait_beside_wanderer(body, from, on_world(wander_wait_plan(step), world, start.map));
    const std::string at = Pokemon::to_string(result.at);
    switch (result.end){
    case WanderWaitEnd::TALKED:
        return Pokemon::outcome_ok();
    case WanderWaitEnd::NO_TALK:
        return Pokemon::outcome_failed("operation_failed", std::format("{}: no talk from any stand tile; on {}.", step.name, at));
    case WanderWaitEnd::STUCK:
        return Pokemon::outcome_failed("operation_failed", std::format("{}: a step along the column stayed blocked at {}.", step.name, at));
    case WanderWaitEnd::LOST:
        return Pokemon::outcome_failed("operation_failed", std::format("{}: a step from the exit moved to {}: not on the column.", step.name, at));
    }
    return Pokemon::outcome_failed("operation_failed", step.name + ": unknown wait end.");
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
