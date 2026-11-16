/*  Overworld Gated Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_GateState.h"
#include "Pokemon/Navigation/Pokemon_GateTiles.h"
#include "PokemonFRLG_OverworldGatedRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


Pokemon::GateState gates_after_route(const Pokemon::GateState& before, const std::vector<RouteStep>& route){
    Pokemon::GateState gates = before;
    for (const RouteStep& step : route){
        Pokemon::apply_gate_change(gates, step.gate_change);
    }
    return gates;
}

bool has_gates(const std::vector<MapData>& maps){
    return std::ranges::any_of(maps, [](const MapData& map){
        return !map.gates.empty();
    });
}

std::vector<MapData> resolve_maps(const std::vector<MapData>& maps, const Pokemon::GateState& gates){
    std::vector<MapData> resolved = maps;
    for (MapData& map : resolved){
        if (map.gates.empty()){
            continue;
        }
        map.tiles = Pokemon::resolve_tiles(map, gates);
        std::erase_if(map.objects, [&](const MapObject& object){
            const Pokemon::MapGate* gate = Pokemon::gate_at(map, TilePos{object.x, object.y});
            return gate != nullptr && Pokemon::gate_is_open(map, *gate, gates);
        });
    }
    return resolved;
}

std::optional<std::string> cut_gate_key(const std::vector<MapData>& maps, const MapPosition& tree){
    for (const MapData& map : maps){
        if (map.name != tree.map || !map.tiles.contains(tree.point)){
            continue;
        }
        const Pokemon::MapGate* gate = Pokemon::gate_at(map, tree.point);
        if (gate != nullptr && gate->kind == Pokemon::GateKind::CUT){
            return Pokemon::gate_key(map.name, gate->id);
        }
    }
    return std::nullopt;
}

void regrow_cut_gates(const std::vector<MapData>& maps, Pokemon::GateState& gates){
    for (const MapData& map : maps){
        for (const auto& [id, gate] : map.gates){
            if (gate.kind == Pokemon::GateKind::CUT){
                gates.opened.erase(Pokemon::gate_key(map.name, id));
            }
        }
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
