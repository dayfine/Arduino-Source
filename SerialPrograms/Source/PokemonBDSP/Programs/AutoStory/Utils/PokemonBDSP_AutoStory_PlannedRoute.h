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
#include "PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"
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
    //  The pace of legs to a wall, which their tile counts are timed by.
    MoveSpeed speed = MoveSpeed::WALK;
    //  The story's next step is a scene: a dialogue no trainer explains is
    //  that scene, started early by a trigger on the way (00.04's Mom's
    //  call, lab-B 2026-11-09 17:29). The walk ends there with it open,
    //  for the scene's step, instead of pressing through it as a trainer's.
    bool scene_next = false;
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
//  walk_route() through `io`.
std::vector<Pokemon::TilePos> walk_route(
    RouteIO& io, const StoryWorld& maps, std::vector<Pokemon::TilePos> from,
    RouteTrainers& trainers, const RouteWalkPlan& plan
);

//  Where walk_route_to_warp() ended: on `tiles`, or through a warp it saw
//  (`warp`), with `tiles` where the leg that went through began.
struct RouteEnd{
    std::vector<Pokemon::TilePos> tiles;
    WarpSeen warp = {};
    //  The next step's scene started on the way (RouteWalkPlan::scene_next):
    //  its dialogue is open, and `tiles` are where the walk may have stopped.
    bool scene = false;
};
//  walk_route(), but a fade on a leg with a warp at most a tile or two past
//  its end ends the walk there, as arrived early: the walk ran on onto the
//  warp (01.05's Oreburgh Gate, lab-B 2026-11-08), and the step after goes
//  through it (#268 slice 2). Any other fade fails as in walk_route().
RouteEnd walk_route_to_warp(
    RouteIO& io, const StoryWorld& maps, std::vector<Pokemon::TilePos> from,
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
//  talk_to_npc() through `io`.
void talk_to_npc(
    RouteIO& io, const StoryWorld& maps, std::vector<Pokemon::TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
);

//  A leg held toward a wall the map has between `fewest` and `most` tiles
//  ahead (from each tile the player may stand on), its tiles counted by time
//  at `speed`.
struct CountedWallLeg{
    Pokemon::Direction direction;
    size_t fewest;
    size_t most;
    MoveSpeed speed;
    std::string label;
};
//  Walks `leg` to its wall and returns about how many tiles it took. Someone
//  walking about who stops it short is waited for, and the walk goes on
//  (Oreburgh's Hiker crosses column 46 on rows 25 to 27). Fails when the
//  count is far off the map's: a walk that went astray is not at its wall
//  (lab-B 2026-11-08 13:50: a hold east from (48, 57) moved 0 where the map
//  has (49, 57) open), nor is one through a door or into a dialogue.
size_t walk_counted_wall_leg(RouteIO& io, const CountedWallLeg& leg);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
