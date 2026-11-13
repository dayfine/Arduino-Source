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
#include "PokemonFRLG_OverworldPress.h"
#include "PokemonFRLG_OverworldRouteCosts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



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
//  A route with gates (plan_overworld_route_gated()) starts with `gates`; its
//  OPEN_GATE and PRESS_SWITCH steps change them.
MapPosition follow_overworld_route(
    const std::vector<MapData>& maps,
    const MapPosition& start,
    const std::vector<RouteStep>& route,
    const Pokemon::GateState& gates = {}
);


//  Registers overworld route unit tests.
void add_tests_OverworldRoute(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
