/*  Gate Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_GateTiles.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string switch_key(const MapSwitch& pressed){
    return pressed.floors.size() == 1 ? gate_key(pressed.floors.front(), pressed.id) : pressed.id;
}

const MapSwitch* switch_with_button(const MapData& map, const TilePos& pos){
    for (const auto& [id, result] : map.switches){
        for (const TilePos& button : result.buttons){
            if (button == pos){
                return &result;
            }
        }
    }
    return nullptr;
}

const MapGate* gate_at(const MapData& map, const TilePos& pos){
    for (const auto& [id, gate] : map.gates){
        for (const TilePos& tile : gate.tiles){
            if (tile == pos){
                return &gate;
            }
        }
    }
    return nullptr;
}

bool gate_is_open(const MapData& map, const MapGate& gate, const GateState& state){
    if (gate.kind == GateKind::SWITCH){
        const auto found = map.switches.find(gate.switch_id);
        const std::string key = found == map.switches.end() ? gate.switch_id : switch_key(found->second);
        return state.switch_on(key) == gate.open_when_on;
    }
    return gate.open_at_new_game || state.opened.contains(gate_key(map.name, gate.id));
}

TileKind resolved_tile(const MapData& map, const TilePos& pos, const GateState& state){
    const TileKind tile = map.tiles.at(pos);
    if (tile != TileKind::GATE){
        return tile;
    }
    const MapGate* gate = gate_at(map, pos);
    return gate != nullptr && gate_is_open(map, *gate, state) ? gate->open_tile : TileKind::WALL;
}

Grid<TileKind> resolve_tiles(const MapData& map, const GateState& state){
    Grid<TileKind> grid = map.tiles;
    for (int y = 0; y < grid.height; y++){
        for (int x = 0; x < grid.width; x++){
            cell_at(TilePos{x, y}, grid) = resolved_tile(map, TilePos{x, y}, state);
        }
    }
    return grid;
}

const MapGate* closed_card_key_gate_faced(const MapData& map, const TilePos& from, Direction direction, const GateState& state){
    const TilePos ahead = from.next(direction);
    if (!map.tiles.contains(ahead) || map.tiles.at(ahead) != TileKind::GATE){
        return nullptr;
    }
    const MapGate* gate = gate_at(map, ahead);
    if (gate == nullptr || gate->kind != GateKind::CARD_KEY || gate_is_open(map, *gate, state)){
        return nullptr;
    }
    return gate;
}

const MapSwitch* switch_pressed(const MapData& map, const TilePos& from, Direction direction){
    for (const auto& [id, result] : map.switches){
        for (const MapStatue& statue : result.statues){
            if (statue.face_from == from && statue.facing == direction){
                return &result;
            }
        }
    }
    return nullptr;
}



} // namespace Pokemon
} // namespace PokemonAutomation
