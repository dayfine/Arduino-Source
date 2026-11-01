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
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
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
//  Running (B held) covers a tile in this fraction of the walking time. B
//  held with the D-pad did not run: r238a walked the Ravaged Path's corridor
//  at 160 ms a tile, and a limit set for twice that speed cut a wall leg
//  short by two tiles. 1 until running is measured to work.
constexpr int ROUTE_RUN_SPEEDUP = 1;

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

//  Talks to someone straight ahead in `facing`, where we may stand a little
//  off their tile: BDSP moves by any distance, and a sideways tap moves about
//  a third of a tile (a 50ms tap 0.2 to 0.45 tiles, a 100ms turn tap 0.4, on
//  the console 2026-10-31). Presses A, and while nobody answers shifts by one
//  such tap at a time to -1, +1, -2, +2 taps from the start and tries again.
//  Never turns away and back between tries: each turn is a tap that moves us
//  (s03e2 drifted east of Roark a third of a tile per try). Returns whether a
//  dialogue opened, left open; false also after a wild battle on a nudge,
//  which leaves us off the place we were.
//  `accept`, if given, looks at the screen before A and once the dialogue
//  is open, and says whether it is the one wanted: a rock or a sign next to
//  someone answers A too (r187a read "It's a rugged rock" beside Roark). A
//  dialogue it turns down is closed with B and the nudges go on.
bool talk_with_nudges(
    VideoStream& stream, ProControllerContext& context,
    Pokemon::Direction facing, const std::string& label,
    Milliseconds wait = Milliseconds(3000),
    const std::function<bool(const ImageViewRGB32& before, const ImageViewRGB32& after)>& accept = nullptr
);
constexpr Milliseconds ROUTE_NUDGE_TAP = Milliseconds(50);

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
Pokemon::RobustPlanCosts route_plan_costs();

std::vector<Pokemon::TilePos> walk_route(
    VideoStream& stream, ProControllerContext& context,
    const Pokemon::MapSet& maps,
    std::vector<Pokemon::TilePos> from,
    const std::function<bool(const Pokemon::TilePos&)>& goal,
    std::vector<Pokemon::TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const std::vector<Pokemon::TilePos>& avoid = {},
    const Pokemon::RobustPlanCosts& costs = route_plan_costs()
);

//  Plan costs between two corners of a walked line: whole tiles on the D-pad
//  land exactly, so an open leg costs nothing extra and the straight leg to
//  the corner beats a run to a wall and a tap back (r244a: "west 10 to the
//  wall, east 1" through the wandering Youngster lost the count; "west 9"
//  in taps keeps it).
Pokemon::RobustPlanCosts waypoint_plan_costs();

//  Walks `from` to each of `waypoints` in turn, with walk_route() for every
//  stretch, and returns where the last one ended. The corners of a line a
//  human walked, so the plan is that line, with the walker's handling of
//  trainers and blocks on each stretch (Route 204 North, 2026-10-31).
std::vector<Pokemon::TilePos> walk_waypoints(
    VideoStream& stream, ProControllerContext& context,
    const Pokemon::MapSet& maps,
    std::vector<Pokemon::TilePos> from,
    const std::vector<Pokemon::TilePos>& waypoints,
    std::vector<Pokemon::TrainerSight>& trainers,
    const RouteTrainerFight& fight,
    const std::string& label,
    const Pokemon::RobustPlanCosts& costs = waypoint_plan_costs()
);

//  Plan costs for walking in town: a one-tile step lands exactly, a short
//  open leg a tile either way, and both cost little more than a leg to a
//  wall, so a plan
//  steps a tile across and walks straight through a gap rather than going
//  round the town to end every leg on a wall (Sandgem: 13 legs, r127a and
//  r128b walked into the Lab and the Mart). Pair it with a check at the end
//  (a sign read) for the timed legs.
Pokemon::RobustPlanCosts town_plan_costs();

//  The costs walk_route() plans with unless told otherwise: open legs are
//  whole tiles on the D-pad, so they land exactly at any length; a wall at
//  a leg's end is still worth a little, and each open tile a little more.
Pokemon::RobustPlanCosts route_plan_costs();

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
//  talk still up and walks south out of it, stopping at the door's fade or
//  the first look that is not a room, so it ends a tile or so outside. Out is
//  then three standing looks in a row that are not a room: one look of a lit
//  Center can miss its black edge (r259). Fails with a screenshot if five
//  walks south get nowhere.
void leave_door_south(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  Brightness (mean r + g + b) below which a look is the door's fade, and
//  above which a look that is not a room is outside.
inline constexpr double DOOR_FADE_DARK = 120.0;
inline constexpr double DOOR_OUTSIDE_BRIGHT = 200.0;

//  What leave_door_south() has seen of the door's fade so far.
struct DoorFadeWatch{
    bool saw_dark = false;
};
//  Takes one look (its brightness, and whether indoors() read a room) and
//  returns whether we are out: a dark look seen, and now a bright one that
//  is not a room.
bool door_fade_look(DoorFadeWatch& watch, double brightness, bool room);



}
}
}
#endif
