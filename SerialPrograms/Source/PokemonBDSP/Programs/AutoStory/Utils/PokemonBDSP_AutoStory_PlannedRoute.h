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

//  The trainers a route walk may meet, and how to fight them. A trainer who
//  is fought is taken off `sights`.
struct RouteTrainers{
    std::vector<TrainerSight> sights;
    RouteTrainerFight fight;
};

//  Where walk_route() goes: until every tile the player may be on is
//  `goal`. Tiles in `avoid` (where someone wanders, a trainer's sight) are
//  planned as off the map.
struct RouteWalkPlan{
    std::function<bool(const Pokemon::TilePos&)> goal;
    std::string label;
    std::vector<Pokemon::TilePos> avoid = {};
    RobustPlanCosts costs = route_plan_costs();
};

//  Walks from any of the world tiles `from` of `maps` until every tile the
//  player may be on is `plan.goal`, and returns those tiles. Trainers who
//  see the player are fought and taken off `trainers`. Fails with a
//  screenshot on a transition, a dialogue no trainer explains, or when no
//  plan exists. A walk from one map to another must see the second map's
//  location banner on the way: a walk whose legs all ended on walls can
//  still end in the wrong place (r124a saved in Sandgem Town after
//  "arriving" on Route 202).
std::vector<Pokemon::TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<Pokemon::TilePos> from,
    RouteTrainers& trainers, const RouteWalkPlan& plan
);

//  The corners of a line a human walked, and the costs to plan each
//  stretch with.
struct WaypointWalkPlan{
    std::vector<Pokemon::TilePos> waypoints;
    std::string label;
    RobustPlanCosts costs = waypoint_plan_costs();
};

//  Walks `from` to each of `plan.waypoints` in turn, with walk_route() for
//  every stretch, and returns where the last one ended: the plan is the
//  line, with the walker's handling of trainers and blocks on each stretch
//  (Route 204 North, 2026-10-31).
std::vector<Pokemon::TilePos> walk_waypoints(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<Pokemon::TilePos> from,
    RouteTrainers& trainers, const WaypointWalkPlan& plan
);

//  Whom talk_to_npc() talks to: the NPC on world tile `npc`, from the sides
//  in `sides` (the side of the NPC to stand on), in order.
struct NpcTalk{
    Pokemon::TilePos npc;
    std::vector<Pokemon::Direction> sides;
    std::string label;
};

//  Walks next to `talk.npc`, faces them and presses A, trying each side
//  until a dialogue opens. `at` is where the player may be, and is updated.
//  Fails with a screenshot if no side starts a dialogue. A press from a
//  diagonal tile, or from a tile a row off, talks to nobody, so each side is
//  tried in turn.
void talk_to_npc(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<Pokemon::TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
);



}
}
}
#endif
