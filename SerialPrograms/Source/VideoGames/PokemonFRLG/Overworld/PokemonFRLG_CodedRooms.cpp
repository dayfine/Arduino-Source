/*  Coded Rooms and Tiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <map>
#include "PokemonFRLG_CodedRooms.h"
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A floor's spinner tiles, by the way each slides the player.
struct SpinnerFloor{
    std::string_view map;
    std::vector<TilePos> north;
    std::vector<TilePos> south;
    std::vector<TilePos> west;
    std::vector<TilePos> east;
    std::vector<TilePos> stop;
};

//  Read off pokefirered: each floor's layout (data/layouts/RocketHideout_
//  B2F, _B3F map.bin, primary tileset gTileset_Building, secondary
//  gTileset_SilphCo) with each metatile's behaviour from the tilesets'
//  metatile_attributes.bin: MB_SPIN_UP (0x56), MB_SPIN_DOWN (0x57),
//  MB_SPIN_LEFT (0x55), MB_SPIN_RIGHT (0x54), MB_STOP_SPINNING (0x58).
const std::vector<SpinnerFloor>& spinner_floors(){
    static const std::vector<SpinnerFloor> FLOORS{
        {
            "RocketHideout_B2F",
            {{9, 5}, {11, 6}, {7, 7}, {11, 8}, {15, 9}, {7, 10}, {9, 10}, {3, 11}, {15, 11}, {13, 12}, {15, 13}, {9, 14}, {5, 17}, {7, 18}, {5, 19}},
            {{12, 5}, {8, 9}, {10, 9}, {14, 11}, {10, 13}, {8, 17}},
            {{3, 4}, {7, 4}, {9, 4}, {11, 4}, {16, 5}, {16, 6}, {12, 13}, {3, 14}, {7, 14}, {12, 14}, {12, 17}, {12, 18}},
            {{3, 6}, {12, 7}, {4, 9}, {3, 10}, {10, 11}, {12, 11}, {9, 12}, {11, 12}, {9, 20}},
            {{1, 4}, {7, 6}, {13, 7}, {15, 8}, {13, 10}, {8, 11}, {14, 13}, {1, 14}, {5, 15}, {10, 15}, {8, 19}, {13, 20}},
        },
        {
            "RocketHideout_B3F",
            {{8, 11}, {10, 14}, {6, 15}, {6, 17}, {4, 18}},
            {{9, 10}, {7, 16}},
            {{4, 9}},
            {{6, 9}, {2, 11}, {6, 13}, {5, 14}, {7, 14}, {4, 15}, {3, 16}, {2, 17}},
            {{2, 9}, {8, 9}, {6, 11}, {10, 13}, {9, 14}, {7, 20}},
        },
    };
    return FLOORS;
}

const SpinnerFloor* spinner_floor(std::string_view map){
    for (const SpinnerFloor& floor : spinner_floors()){
        if (floor.map == map){
            return &floor;
        }
    }
    return nullptr;
}

bool contains(const std::vector<TilePos>& tiles, const TilePos& point){
    return std::any_of(tiles.begin(), tiles.end(), [&](const TilePos& tile){
        return tile.x == point.x && tile.y == point.y;
    });
}

bool has_object(const MapData& map, std::string_view name){
    return std::any_of(map.objects.begin(), map.objects.end(), [&](const MapObject& object){
        return object.name == name;
    });
}

MapObject object(std::string name, Pokemon::ObjectKind kind, const TilePos& tile){
    MapObject out;
    out.name = std::move(name);
    out.kind = kind;
    out.x = tile.x;
    out.y = tile.y;
    return out;
}

//  LAYOUT_POKEMON_CENTER_1F's collision and elevation, as the map lock's
//  seven Centers (PokemonSimulation 6c2f5c8, data/games/frlg/maps/runtime/
//  *_PokemonCenter_1F.json) all have them. The mat's warps: (6..8, 8),
//  the middle one MB_SOUTH_ARROW_WARP-like ("walk_south"); the stairs to
//  2F at (1, 6), which isn't loaded.
const char CENTER_ROOM_JSON[] = R"({
    "name": "PokemonCenter_1F",
    "source": {"decomp_file": "data/layouts/PokemonCenter_1F/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-09"},
    "allow_running": false,
    "tiles": ["#.#############", "###############", "....###..##....", "....#######....", "...............", "...............", "#..........##..", "...........##..", "...............", "###############"],
    "elevation": ["030000000000000", "000000000000000", "333300033003333", "333300000003333", "333333333333333", "330333333333333", "044033333330033", "330333333330033", "333333333333333", "000000000000000"],
    "points": {"warp_0": [6, 8], "warp_1": [7, 8], "warp_2": [8, 8], "nurse_front": [7, 4]},
    "warps": [
        {"x": 6, "y": 8, "enter": "walk_into"},
        {"x": 7, "y": 8, "enter": "walk_south"},
        {"x": 8, "y": 8, "enter": "walk_into"},
        {"x": 1, "y": 6, "enter": "step_on"}
    ],
    "objects": [{"name": "nurse", "kind": "npc", "x": 7, "y": 2}]
})";

//  LAYOUT_ROCKET_HIDEOUT_ELEVATOR's collision (map.bin: rows 0 to 2 and 6
//  walls, 3 to 5 floor), the panel's bg event at (0, 2), and warp 1 at
//  (2, 5), the one MB_SOUTH_ARROW_WARP tile (warp 0 at (1, 5) has no
//  behaviour, so never fires).
const char LIFT_ROOM_JSON[] = R"({
    "name": "RocketHideout_Elevator",
    "source": {"decomp_file": "data/layouts/RocketHideout_Elevator/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-09"},
    "tiles": ["#####", "#####", "S####", ".....", ".....", ".....", "#####"],
    "points": {"warp_0": [1, 5], "warp_1": [2, 5]},
    "warps": [{"x": 2, "y": 5, "enter": "walk_south"}]
})";

} // namespace


std::optional<Direction> coded_spin_direction(std::string_view map, const TilePos& point){
    const SpinnerFloor* floor = spinner_floor(map);
    if (floor == nullptr){
        return std::nullopt;
    }
    if (contains(floor->north, point)){
        return Direction::NORTH;
    }
    if (contains(floor->south, point)){
        return Direction::SOUTH;
    }
    if (contains(floor->west, point)){
        return Direction::WEST;
    }
    if (contains(floor->east, point)){
        return Direction::EAST;
    }
    return std::nullopt;
}

bool coded_spin_stop(std::string_view map, const TilePos& point){
    const SpinnerFloor* floor = spinner_floor(map);
    return floor != nullptr && contains(floor->stop, point);
}


void add_coded_objects(MapData& map){
    if (map.name != GAME_CORNER_MAP){
        return;
    }
    if (!has_object(map, "poster")){
        map.objects.push_back(object("poster", Pokemon::ObjectKind::SIGN, GAME_CORNER_POSTER));
    }
    if (!has_object(map, "poster_stairs")){
        for (const TilePos& tile : {TilePos{15, 2}, TilePos{16, 2}, TilePos{17, 2}, TilePos{16, 3}, TilePos{17, 3}}){
            map.objects.push_back(object("poster_stairs", Pokemon::ObjectKind::NPC, tile));
        }
    }
}


bool is_pokemon_center(std::string_view map){
    return map.ends_with("_PokemonCenter_1F");
}

MapData pokemon_center_room(const std::string& name, const Pokemon::WarpDestination& outside){
    MapData room = parse_map(CENTER_ROOM_JSON, name);
    room.name = name;
    for (MapWarp& warp : room.warps){
        if (warp.y == CENTER_MAT.y){
            warp.to = outside;
        }
    }
    return room;
}

std::optional<std::string> add_pokemon_center_room(std::vector<MapData>& maps, size_t town){
    for (const MapWarp& warp : maps[town].warps){
        if (!warp.to || !is_pokemon_center(warp.to->map)){
            continue;
        }
        const std::string center = warp.to->map;
        const std::string door = center + "_door";
        maps[town].points[door] = TilePos{warp.x, warp.y};
        Pokemon::WarpDestination outside{maps[town].name, door};
        maps.push_back(pokemon_center_room(center, outside));
        return center;
    }
    return std::nullopt;
}


MapData rocket_hideout_lift_room(){
    return parse_map(LIFT_ROOM_JSON, std::string(LIFT_MAP));
}

const std::vector<MapPosition>& rocket_hideout_lift_doors(){
    static const std::vector<MapPosition> DOORS{
        {"RocketHideout_B1F", {24, 25}},
        {"RocketHideout_B2F", {28, 16}},
        {"RocketHideout_B4F", {20, 23}},
    };
    return DOORS;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
