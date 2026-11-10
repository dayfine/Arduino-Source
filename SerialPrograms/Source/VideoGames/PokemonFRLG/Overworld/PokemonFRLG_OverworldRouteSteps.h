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
    //  WALK: a wall stops the player at the end, so the leg can be held for
    //  longer than it takes and still ends on its tile.
    bool to_wall = false;
    //  WALK, SURF: a wild Pokemon can appear on the way: the step crosses
    //  tall grass or water. Elsewhere (indoors, streets) no wild battle can
    //  start, so the walker doesn't watch for one.
    bool wild = false;
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
