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

//  How long to hold the stick for an open leg of `tiles` tiles, for a player
//  already facing that way: a step once started is always finished, so stop
//  half a tile early. On the console a 1290ms hold west went 9 tiles and an
//  800ms hold south 4: the player starts at once and finishes the step.
Milliseconds route_open_hold(Pokemon::GridDirection direction, size_t tiles);

//  A tap this short turns a player who faces another way, without a step: on
//  the console a 150ms tap south after walking north only turned the player.
constexpr Milliseconds ROUTE_TURN_TAP = Milliseconds(100);

//  Turns the player to face `direction` without moving. Only for a player who
//  faces another way: one already facing it may take a step.
void route_turn(VideoStream& stream, ProControllerContext& context, Pokemon::GridDirection direction);

//  Shows what the route walk is doing in the log and the video overlay.
void route_status(VideoStream& stream, const std::string& text);

//  Called when a trainer's dialogue has opened, or when someone standing in
//  the way was talked to. Fights the battle if one starts, and returns to the
//  overworld either way.
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

//  Walks next to the NPC on world tile `npc`, faces them and presses A, trying
//  the sides in `sides` (the side of the NPC to stand on) in order until a
//  dialogue opens. `at` is where the player may be, and is updated. Fails with
//  a screenshot if no side starts a dialogue. A press from a diagonal tile,
//  or from a tile a row off, talks to nobody, so each side is tried in turn.
void talk_to_npc(
    VideoStream& stream, ProControllerContext& context,
    const Pokemon::MapSet& maps,
    std::vector<Pokemon::TilePos>& at,
    const Pokemon::TilePos& npc,
    const std::vector<Pokemon::GridDirection>& sides,
    std::vector<Pokemon::TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
);

//  Finishes a dialogue that has opened: `a_presses` presses of A, 1.5 s
//  apart (A answers Yes and closes "obtained" messages), then B until the
//  overworld is back with no dialogue. B never starts a new talk, while an A
//  after the last line talks to the NPC again.
void finish_dialogue(
    VideoStream& stream, ProControllerContext& context,
    size_t a_presses, const std::string& label
);

//  For a player on the tile below a door: walks north through it. If a wall
//  stops the walk (the door frame, a tile off the opening), steps a tile east,
//  then west, and tries again. Fails with a screenshot if no step goes in.
void enter_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label);



}
}
}
#endif
