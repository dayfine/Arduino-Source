/*  Overworld Gated Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans a route over FRLG maps that have gates (Pokemon_GateState.h): Silph
 *  Co.'s card-key doors and Pokemon Mansion's statue switch. The game's
 *  step rules (plan_overworld_route()) are wrapped by the shared gated
 *  planner (Pokemon_GatedPlanner.h), so a closed gate is a wall, and a route
 *  may open a card-key gate (RouteAction::OPEN_GATE) when the story says the
 *  player holds the Card Key, or press a statue (RouteAction::PRESS_SWITCH).
 *  The gates after the route are gates_after_route().
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldGatedRoute_H
#define PokemonAutomation_PokemonFRLG_OverworldGatedRoute_H

#include <optional>
#include <vector>
#include "Pokemon/Navigation/Pokemon_GatedTypes.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  plan_overworld_route() with the gates as `gates` says and `rules` allows.
//  Without gates in `maps` the route is the same.
std::optional<std::vector<RouteStep>> plan_overworld_route_gated(
    const std::vector<MapData>& maps,
    const MapPosition& start,
    const RouteGoal& goal,
    const Pokemon::GateState& gates,
    const Pokemon::GateRules& rules,
    const RouteCosts& costs = default_route_costs(),
    std::optional<Direction> start_facing = std::nullopt
);

//  `before` with the gates `route` opens and the switches it flips.
Pokemon::GateState gates_after_route(const Pokemon::GateState& before, const std::vector<RouteStep>& route);

//  Whether any of `maps` has a gate (MapData::gates).
bool has_gates(const std::vector<MapData>& maps);

//  `maps` with the gate tiles of each as `gates` makes them: a closed gate
//  is a wall (a GATE tile), an open one its open tile. What a walk reads
//  its tiles from, as the maps stay as loaded.
std::vector<MapData> resolve_maps(const std::vector<MapData>& maps, const Pokemon::GateState& gates);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
