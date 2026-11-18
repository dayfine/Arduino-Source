/*  Overworld Route Revisits
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The tiles a planned route steps on twice: a route that crosses its own
 *  way walks back over it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRouteRevisits_H
#define PokemonAutomation_PokemonFRLG_OverworldRouteRevisits_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_GateState.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The tiles `route` from `start` steps on again, in the order it does: a
//  route that crosses a tile twice walks back over its own way. Counts the
//  tiles leg_tiles() walks, each warp's and spinner's end, and each Surf's
//  landing. The gates start as `gates`, and the route's OPEN_GATE and
//  PRESS_SWITCH steps change them, as in follow_overworld_route().
std::vector<MapPosition> revisited_tiles(
    const std::vector<MapData>& maps,
    const MapPosition& start, const std::vector<RouteStep>& route,
    const Pokemon::GateState& gates
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
