/*  Overworld Route Revisits
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <vector>
#include "PokemonFRLG_OverworldGatedRoute.h"
#include "PokemonFRLG_OverworldRouteRevisits.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::vector<MapPosition> revisited_tiles(
    const std::vector<MapData>& maps,
    const MapPosition& start, const std::vector<RouteStep>& route,
    const Pokemon::GateState& start_gates
){
    Pokemon::GateState gates = start_gates;
    const bool gated = has_gates(maps);
    std::vector<MapData> resolved;
    if (gated){
        resolved = resolve_maps(maps, gates);
    }
    std::vector<MapPosition> visited{start};
    std::vector<MapPosition> revisited;
    const auto visit = [&](const MapPosition& tile){
        if (std::ranges::find(visited, tile) != visited.end()){
            revisited.push_back(tile);
        }else{
            visited.push_back(tile);
        }
    };
    MapPosition at = start;
    for (const RouteStep& step : route){
        if (step.action == RouteAction::OPEN_GATE || step.action == RouteAction::PRESS_SWITCH){
            Pokemon::apply_gate_change(gates, step.gate_change);
            if (gated){
                resolved = resolve_maps(maps, gates);
            }
        }else if (step.action == RouteAction::WALK){
            const std::vector<MapPosition> tiles = leg_tiles(gated ? resolved : maps, at, step);
            for (size_t c = 1; c < tiles.size(); c++){
                visit(tiles[c]);
            }
            if (!(tiles.back() == step.end)){
                visit(step.end);
            }
        }else if (step.action == RouteAction::SURF){
            visit(step.end);
        }
        at = step.end;
    }
    return revisited;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
