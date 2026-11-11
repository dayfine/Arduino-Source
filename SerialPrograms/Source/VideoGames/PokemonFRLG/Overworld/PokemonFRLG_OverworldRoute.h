/*  Overworld Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans a route over one or more FRLG maps joined by warps: the D-pad legs
 *  to walk, where to start surfing, and which way to face at the end. FRLG
 *  moves exactly one tile per step, so a planned route can be walked blind.
 *  Legs end against walls where that costs little: such a leg can be held
 *  for longer than it takes, and walked again if something interrupts it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRoute_H
#define PokemonAutomation_PokemonFRLG_OverworldRoute_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Position on a specific map.
struct MapPosition{
    std::string map;
    TilePos point;

    bool operator==(const MapPosition& other) const = default;
};
//  Formats a MapPosition as a string.
std::string to_string(const MapPosition& position);


//  Action associated with reaching a route waypoint.
enum class RouteAction{
    WALK,       //  hold `direction` for `tiles` tiles
    SURF,       //  face `direction`, use Surf, and land on the water there
    FACE,       //  turn to `direction` without moving
};

//  A stationary prediction in the map's runtime-local tile/elevation frame.
struct OverworldPose{
    MapPosition position;
    uint8_t elevation = ELEVATION_GROUND;
    bool surfing = false;
    Direction facing = Direction::NORTH;
};
//  One transition from the production planner's own collision/warp implementation.
struct OverworldPress{
    OverworldPose pose;
    bool warped = false;
};
//  A model prediction only; nullopt means no known move, never confirmed localization.
std::optional<OverworldPress> predict_overworld_press(
    const std::vector<MapData>& maps, const OverworldPose& from, Direction direction
);

//  A single step along an overworld path.
struct RouteStep{
    RouteAction action = RouteAction::WALK;
    Direction direction = Direction::NORTH;
    int tiles = 0;
    //  WALK: the last tile is a warp (ladder, stairs), so the screen fades
    //  and the player arrives on another map.
    bool warps = false;
    //  WALK: on the water, where the player moves twice as fast.
    bool surfing = false;
    //  WALK: on a map where the game doesn't let the player run (indoors,
    //  MapData::allow_running): B held there still walks.
    bool walk_only = false;
    //  WALK: a wall stops the player at the end, so the leg can be held for
    //  longer than it takes and still ends on its tile.
    bool to_wall = false;
    //  WALK: the last tile is a spinner, which slides the player this many
    //  tiles on, to `end` (spin_slide()). The hold must end on the spinner:
    //  held on, the player walks on after the slide.
    std::optional<int> spin_tiles;
    //  WALK on foot: walked a tap at a time, each a tile, and each checked
    //  to have moved (count_step_taps()), as through the hideout's arrow
    //  floors, where a hold that ends a tile off slides the player
    //  elsewhere (console B, 2026-11-10, 09.01 r1).
    bool tapped = false;
    //  WALK, tapped: played blind, this many taps (the leg's tiles, and a
    //  turn tap first if the player faced another way), each waited out
    //  (step_tap_settle()) and never checked, the end trusted: the
    //  hideout's spinner mazes, where a slide read as no move was tapped
    //  again and re-planned off the route (console B, 2026-11-10 22:35).
    //  0: not blind.
    int blind_taps = 0;
    //  WALK, warps: the warp's fade only dims the screen, never black (the
    //  hideout lift's way out; console B, 2026-11-10, 09.02 lift_to_b4f).
    bool fade_dims = false;
    //  WALK, SURF: a wild Pokemon can appear on the way: the step crosses
    //  tall grass or water. Elsewhere (indoors, streets) no wild battle can
    //  start, so the walker doesn't watch for one.
    bool wild = false;
    //  Which way the player faces after the step. Arriving at a door or a
    //  cave mouth, the game walks the player a tile out, facing south.
    Direction facing_after = Direction::NORTH;
    //  Where the player is after the step.
    MapPosition end;
    //  WALK: the steps (from 1) onto a connected map. The game holds the
    //  player up a tile before each while it loads the map: a hold timed
    //  by the tiles alone ended a tile short (console B, 2026-11-03, rows
    //  04.06, 05.05 and 05.09).
    std::vector<int> connection_steps = {};
    //  WALK onto a warp: how many tiles further the player may stand from
    //  it than planned, which the hold goes on for, stopping at the fade.
    //  An open leg over a map connection before it may end a tile further
    //  on, when the game doesn't hold the player up there (console B,
    //  2026-11-03, row 06.03). Set by with_warp_slack() when a route runs.
    int warp_slack = 0;
};
//  Formats a RouteStep as a string.
std::string to_string(const RouteStep& step);


//  Navigation goal specification.
struct RouteGoal{
    MapPosition position;
    //  Which way to face on arrival, e.g. towards a Pokemon to talk to.
    std::optional<Direction> facing;
};

//  What the planner weighs a route by. Each is a cost per occurrence.
enum class RouteCost{
    TILE,
    TURN,           //  extra for a leg in a new direction
    WARP,           //  the fade in and out
    SURF,           //  the dialog and the animation; below 0, never
    GRASS_TILE,     //  extra for tall grass, where wild Pokemon appear
    //  Extra for a leg that ends where the player could keep walking: it has
    //  to be timed, and if a message or a battle stops it part-way, where
    //  the player is can only be estimated. A leg that ends at a wall is
    //  walked again instead.
    OPEN_STOP,
    //  Extra again when that leg crosses tall grass or water, where a wild
    //  battle is likely to stop it: the walker can't tell where it did
    //  (console B, 2026-11-01, Route 1, four runs).
    OPEN_WILD_STOP,
};
//  Cost mapping table for path planning.
using RouteCosts = std::map<RouteCost, int>;

//  TILE 1, TURN 2, WARP 30, SURF 40, GRASS_TILE 20, OPEN_STOP 8,
//  OPEN_WILD_STOP 300.
const RouteCosts& default_route_costs();

//  `costs`'s cost for `kind`, or the default if it has none.
int route_cost(const RouteCosts& costs, RouteCost kind);

//  The cheapest route from `start`, surfing if it is on water, to
//  `goal`, or nothing if there is none. `maps` must hold every map the route
//  may pass through; warps to other maps are not taken. Planned with
//  Pokemon::plan_legs() on the game's step rules.
std::optional<std::vector<RouteStep>> plan_overworld_route(
    const std::vector<MapData>& maps,
    const MapPosition& start,
    const RouteGoal& goal,
    const RouteCosts& costs = default_route_costs(),
    std::optional<Direction> start_facing = std::nullopt
);

//  Holding `direction` from `from`, on the water if `surfing`: where a wall
//  stops the player, how many tiles on, whether any tile on the way is tall
//  grass or water, and whether the map lets the player run.
struct WallHold{
    MapPosition end;
    int tiles = 0;
    bool wild = false;
    bool walk_only = false;
};
//  Nothing unless every step on the way is a plain walk: a warp, Surf, a
//  ledge, a person, a sign that would speak, or the edge of the loaded maps
//  comes first.
std::optional<WallHold> hold_to_wall(
    const std::vector<MapData>& maps,
    const MapPosition& from, bool surfing, Direction direction
);

//  The tiles a WALK `step` from `start` passes through, `start` first, short
//  of a warp at its end.
std::vector<MapPosition> leg_tiles(
    const std::vector<MapData>& maps,
    const MapPosition& start, const RouteStep& step
);

//  Replays `route` from `start` with the game's step rules and returns where
//  it ends. Throws InternalProgramError if a step can't be taken as planned.
MapPosition follow_overworld_route(
    const std::vector<MapData>& maps,
    const MapPosition& start,
    const std::vector<RouteStep>& route
);


//  Registers overworld route unit tests.
void add_tests_OverworldRoute(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
