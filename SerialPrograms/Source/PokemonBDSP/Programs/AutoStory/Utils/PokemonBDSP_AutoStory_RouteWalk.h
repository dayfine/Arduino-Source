/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks a plan made on the story maps: legs to a wall are held until the
 *  player stops, open legs for their length in time. Tracks every tile the
 *  player may be on, and plans again after a trainer stops the walk.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteWalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteWalk_H

#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_MapSet.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Walking time per tile, measured on the console from the post-catch save:
//  a row is deeper than a column is wide under the tilted camera.
constexpr Milliseconds ROUTE_MS_PER_TILE_ACROSS = Milliseconds(160);    //  east or west
constexpr Milliseconds ROUTE_MS_PER_TILE_UP_DOWN = Milliseconds(240);   //  north or south
//  From pressing the stick to the first step, when standing still.
constexpr Milliseconds ROUTE_START_DELAY = Milliseconds(250);

//  How long to hold the stick for an open leg of `tiles` tiles: a step once
//  started is always finished, so stop half a tile early.
Milliseconds route_open_hold(Pokemon::GridDirection direction, size_t tiles);

//  Called when a trainer's dialogue has opened. Fights the battle and returns
//  to the overworld.
using RouteTrainerFight = std::function<void(const std::string& trainer)>;

//  Walks from any of the world tiles `from` of `maps` until every tile the
//  player may be on is `goal`, and returns those tiles. Trainers in
//  `trainers` who see the player are fought with `fight` and taken off the
//  list. Fails with a screenshot on a transition, a dialogue no trainer
//  explains, a wild battle during an open leg, or when no plan exists.
std::vector<Pokemon::TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context,
    const Pokemon::MapSet& maps,
    std::vector<Pokemon::TilePos> from,
    const std::function<bool(const Pokemon::TilePos&)>& goal,
    std::vector<Pokemon::TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
);



}
}
}
#endif
