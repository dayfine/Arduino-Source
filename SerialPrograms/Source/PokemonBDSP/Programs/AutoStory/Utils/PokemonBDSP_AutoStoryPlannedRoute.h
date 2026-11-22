/*  BDSP AutoStory - Planned Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks a plan made on the story maps: legs to a wall are held until the
 *  player stops, open legs are tapped a tile at a time. Tracks every tile
 *  the player may be on, and plans again after something stops the walk.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryPlannedRoute_H
#define PokemonAutomation_PokemonBDSP_AutoStoryPlannedRoute_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryPlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryWorld.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryTrainerSight.h"
#include "PokemonBDSP_AutoStoryRouteIO.h"
#include "PokemonBDSP_AutoStoryRouteWalk.h"
#include "PokemonBDSP_AutoStoryWalk.h"
#include "PokemonBDSP_AutoStoryWalkResult.h"

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
    //  `goal` is that scene's trigger (ROUTE_ONTO_TRIGGER): a walk stops on
    //  it when the scene starts, as at a wall.
    bool goal_is_trigger = false;
    //  Known facing before the first leg, if any. On BDSP a short D-pad turn
    //  tap steps a full tile when already facing that way, so open legs only
    //  send a turn tap when the player is known to face another direction.
    std::optional<Pokemon::Direction> start_facing = std::nullopt;
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
    //  The player's actual facing after the last leg walked, if any.
    std::optional<Pokemon::Direction> facing = std::nullopt;
};
//  walk_route(), but a fade on a leg with a warp at most a tile or two past
//  its end ends the walk there, as arrived early: the walk ran on onto the
//  warp (01.05's Oreburgh Gate, lab-B 2026-11-08), and the step after goes
//  through it (#268 slice 2). Any other fade fails as in walk_route().
RouteEnd walk_route_to_warp(
    RouteIO& io, const StoryWorld& maps, std::vector<Pokemon::TilePos> from,
    RouteTrainers& trainers, const RouteWalkPlan& plan
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
