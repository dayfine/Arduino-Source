/*  Overworld Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans a route over one or more FRLG maps joined by warps: the D-pad legs
 *  to walk, where to start surfing, and which way to face at the end. FRLG
 *  moves exactly one tile per step, so a planned route can be walked blind.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRoute_H
#define PokemonAutomation_PokemonFRLG_OverworldRoute_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


struct MapPosition{
    std::string map;
    MapPoint point;

    bool operator==(const MapPosition& other) const = default;
};
std::string to_string(const MapPosition& position);


enum class RouteAction{
    WALK,       //  hold `direction` for `tiles` tiles
    SURF,       //  face `direction`, use Surf, and land on the water there
    FACE,       //  turn to `direction` without moving
};

struct RouteStep{
    RouteAction action = RouteAction::WALK;
    MapDirection direction = MapDirection::NORTH;
    int tiles = 0;
    //  WALK: the last tile is a warp (ladder, stairs), so the screen fades
    //  and the player arrives on another map.
    bool warps = false;
    //  Where the player is after the step.
    MapPosition end;
};
std::string to_string(const RouteStep& step);


struct RouteGoal{
    MapPosition position;
    //  Which way to face on arrival, e.g. towards a Pokemon to talk to.
    std::optional<MapDirection> facing;
};

struct RouteCosts{
    int per_tile = 1;
    int per_turn = 2;           //  extra for a leg in a new direction
    int per_warp = 30;          //  the fade in and out
    int per_surf = 40;          //  the dialog and the animation
    int per_grass_tile = 20;    //  extra for tall grass, where wild Pokemon appear
};

//  The cheapest route from `start`, standing on land and not surfing, to
//  `goal`, or nothing if there is none. `maps` must hold every map the route
//  may pass through; warps to other maps are not taken.
std::optional<std::vector<RouteStep>> plan_overworld_route(
    const std::vector<OverworldMap>& maps,
    const MapPosition& start,
    const RouteGoal& goal,
    const RouteCosts& costs = RouteCosts()
);

//  Replays `route` from `start` with the game's step rules and returns where
//  it ends. Throws InternalProgramError if a step can't be taken as planned.
MapPosition follow_overworld_route(
    const std::vector<OverworldMap>& maps,
    const MapPosition& start,
    const std::vector<RouteStep>& route
);


void add_tests_OverworldRoute(UnitTestDatabase& database);



}
}
}
#endif
