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
#include <optional>
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
Milliseconds route_open_hold(Pokemon::Direction direction, size_t tiles);

//  A tap this short turns a player who faces another way, without a step: on
//  the console a 150ms tap south after walking north only turned the player.
constexpr Milliseconds ROUTE_TURN_TAP = Milliseconds(100);

//  Turns the player to face `direction` without moving. Only for a player who
//  faces another way: one already facing it may take a step.
void route_turn(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction);

//  Talks to someone on a tile next to ours, without trusting the tile we
//  counted ourselves onto: for each of `directions` in order, turns that way,
//  presses A and waits up to `wait` for a dialogue. Returns the direction that
//  answered, its dialogue open, or none if nobody did. Turns are taps (see
//  route_turn()), so do not list the way the player already faces first.
std::optional<Pokemon::Direction> talk_to_adjacent_npc(
    VideoStream& stream, ProControllerContext& context,
    const std::vector<Pokemon::Direction>& directions,
    const std::string& label, Milliseconds wait = Milliseconds(1500)
);

//  Whether a walk to the wall that ends on any of `after` can tell it has
//  stopped: not in tall grass, whose sway keeps the screen changing at a
//  wall. Elsewhere, a walk to the wall that stops confirms where it ended.
bool route_wall_readable(const Pokemon::TileGrid& grid, const std::vector<Pokemon::TilePos>& after);

//  Shows what the route walk is doing in the log and the video overlay.
void route_status(VideoStream& stream, const std::string& text);

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
    VideoStream& stream, ProControllerContext& context,
    const Pokemon::MapSet& maps,
    std::vector<Pokemon::TilePos> from,
    const std::function<bool(const Pokemon::TilePos&)>& goal,
    std::vector<Pokemon::TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const std::vector<Pokemon::TilePos>& avoid = {},
    const Pokemon::RobustPlanCosts& costs = Pokemon::RobustPlanCosts()
);

//  Plan costs for walking in town: a one-tile step lands exactly, a short
//  open leg a tile either way, and both cost little more than a leg to a
//  wall, so a plan
//  steps a tile across and walks straight through a gap rather than going
//  round the town to end every leg on a wall (Sandgem: 13 legs, r127a and
//  r128b walked into the Lab and the Mart). Pair it with a check at the end
//  (a sign read) for the timed legs.
Pokemon::RobustPlanCosts town_plan_costs();

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
    const std::vector<Pokemon::Direction>& sides,
    std::vector<Pokemon::TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label
);

//  Finishes a dialogue that has opened, however many lines it has: presses
//  `button` while a dialogue box shows, and returns once the overworld has
//  shown with no box for 1.5 s (the box blinks off between lines and during
//  "obtained" jingles). Use A where the talk asks Yes/No or gives an item (A
//  answers Yes and closes "obtained" lines; B would answer No), B elsewhere
//  (B never starts a new talk). Fails with a screenshot after `timeout`.
void finish_dialogue(
    VideoStream& stream, ProControllerContext& context,
    Button button, const std::string& label,
    Milliseconds timeout = std::chrono::seconds(40)
);

//  For a player on the tile below a door: walks north through it. If a wall
//  stops the walk (the door frame, a tile off the opening), steps a tile east,
//  then west, and tries again. Fails with a screenshot if no step goes in.
void enter_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  For a player inside a building whose door is straight south: closes any
//  talk still up and walks south out of it. The door's fade is not always
//  black enough to read as a transition, so a whole new scene also counts.
//  Fails with a screenshot if three walks south get nowhere.
void leave_door_south(VideoStream& stream, ProControllerContext& context, const std::string& label);



}
}
}
#endif
