/*  Overworld Route Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A planned overworld route's parts: positions on a map, the steps a
 *  route is walked in, and the goal it is planned to.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRouteSteps_H
#define PokemonAutomation_PokemonFRLG_OverworldRouteSteps_H

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
    //  WALK, warps: the warp's fade only dims the screen, never black (the
    //  hideout lift's way out; console B, 2026-11-10, 00.32 lift_to_b4f).
    bool fade_dims = false;
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
    //  WALK, SURF: a wild Pokemon can appear on the way: the step crosses
    //  tall grass or water. Elsewhere (indoors, streets) no wild battle can
    //  start, so the walker doesn't watch for one.
    bool wild = false;
    //  WALK, open, north or south: the tile past the end is a sign. A hold
    //  that runs on reads it, so a message after the hold, not a battle,
    //  leaves the player on the end (console B, 2026-11-12, 11.03: "north 5"
    //  to Safari Zone West's (27, 10), below the sign on (27, 9)).
    bool sign_ahead = false;
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
    //  Which way the player faces after the step. Arriving at a door or a
    //  cave mouth, the game walks the player a tile out, facing south.
    Direction facing_after = Direction::NORTH;
    //  Where the player is after the step.
    MapPosition end;
};
//  Formats a RouteStep as a string.
std::string to_string(const RouteStep& step);


//  Navigation goal specification.
struct RouteGoal{
    MapPosition position;
    //  Which way to face on arrival, e.g. towards a Pokemon to talk to.
    std::optional<Direction> facing;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
