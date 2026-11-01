/*  BDSP AutoStory - Planned Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks a plan made on the story maps: legs to a wall are held until the
 *  player stops, open legs are tapped a tile at a time. Tracks every tile
 *  the player may be on, and plans again after something stops the walk.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_PlannedRoute_H
#define PokemonAutomation_PokemonBDSP_AutoStory_PlannedRoute_H

#include <functional>
#include <string>
#include <vector>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Called when a trainer's dialogue has opened, or when someone standing in
//  the way was talked to. Fights the battle if one starts, and returns to the
//  overworld either way.
using RouteTrainerFight = std::function<void(const std::string& trainer)>;

//  Walks from any of the world tiles `from` of `maps` until every tile the
//  player may be on is `goal`, and returns those tiles. Trainers in
//  `trainers` who see the player are fought with `fight` and taken off the
//  list. Tiles in `avoid` (where someone wanders) are planned as walls.
//  Fails with a screenshot
//  on a transition, a dialogue no trainer explains, a wild battle during an
//  open leg, or when no plan exists. A walk from one map to another must see
//  the second map's location banner on the way: a walk whose legs all ended
//  on walls can still end in the wrong place (r124a saved in Sandgem Town
//  after "arriving" on Route 202).
std::vector<Pokemon::TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps,
    std::vector<Pokemon::TilePos> from,
    const std::function<bool(const Pokemon::TilePos&)>& goal,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const std::vector<Pokemon::TilePos>& avoid = {},
    const RobustPlanCosts& costs = route_plan_costs()
);

//  Walks `from` to each of `waypoints` in turn, with walk_route() for every
//  stretch, and returns where the last one ended. The corners of a line a
//  human walked, so the plan is that line, with the walker's handling of
//  trainers and blocks on each stretch (Route 204 North, 2026-10-31).
std::vector<Pokemon::TilePos> walk_waypoints(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps,
    std::vector<Pokemon::TilePos> from,
    const std::vector<Pokemon::TilePos>& waypoints,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const RobustPlanCosts& costs = waypoint_plan_costs()
);

//  Walks next to the NPC on world tile `npc`, faces them and presses A, trying
//  the sides in `sides` (the side of the NPC to stand on) in order until a
//  dialogue opens. `at` is where the player may be, and is updated. Fails with
//  a screenshot if no side starts a dialogue. A press from a diagonal tile,
//  or from a tile a row off, talks to nobody, so each side is tried in turn.
void talk_to_npc(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps,
    std::vector<Pokemon::TilePos>& at,
    const Pokemon::TilePos& npc,
    const std::vector<Pokemon::Direction>& sides,
    std::vector<TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
);



}
}
}
#endif
