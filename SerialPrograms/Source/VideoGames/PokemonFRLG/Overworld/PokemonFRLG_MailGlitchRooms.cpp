/*  Mail Glitch Coded Rooms
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <string>
#include <string_view>
#include "PokemonFRLG_MailGlitchRooms.h"
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  CeladonCity_Restaurant (15x11, LAYOUT_CELADON_CITY_RESTAURANT).
//  Warps (5..7, 9) return to CeladonCity warp_7 (37, 29).
//  CoinCaseMan stands at (1, 2); player talks from (2, 2) facing west.
constexpr std::string_view CELADON_RESTAURANT_JSON = R"({
    "name": "CeladonCity_Restaurant",
    "source": {"decomp_file": "data/layouts/CeladonCity_Restaurant/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-22"},
    "allow_running": false,
    "tiles": [
        "###############",
        "###############",
        "..........#####",
        ".##.......#....",
        ".##.......#....",
        "..........#....",
        "..........#....",
        ".##.......#....",
        ".##.......#####",
        "...............",
        "###############"
    ],
    "elevation": [
        "000000000000000",
        "000000000000000",
        "333333333300000",
        "300333333303333",
        "300333333303333",
        "333333333303333",
        "333333333303333",
        "300333333303333",
        "300333333300000",
        "333333333333333",
        "000000000000000"
    ],
    "points": {"warp_0": [5, 9], "warp_1": [6, 9], "warp_2": [7, 9], "coin_case_front": [2, 2]},
    "warps": [
        {"x": 5, "y": 9, "enter": "walk_into", "to_map": "CeladonCity", "to_point": "warp_7"},
        {"x": 6, "y": 9, "enter": "walk_south", "to_map": "CeladonCity", "to_point": "warp_7"},
        {"x": 7, "y": 9, "enter": "walk_into", "to_map": "CeladonCity", "to_point": "warp_7"}
    ],
    "objects": [
        {"name": "coin_case_man", "kind": "npc", "x": 1, "y": 2},
        {"name": "fat_man", "kind": "npc", "x": 9, "y": 4},
        {"name": "worker_m", "kind": "npc", "x": 3, "y": 7},
        {"name": "woman", "kind": "npc", "x": 11, "y": 4},
        {"name": "chef", "kind": "npc", "x": 12, "y": 7}
    ]
})";

//  CeladonCity_GameCorner_PrizeRoom (9x10, LAYOUT_CELADON_CITY_GAME_CORNER_PRIZE_ROOM).
//  Warp (4, 8) walk_south returns to CeladonCity warp_5 (39, 20).
//  PrizeClerkMons stands at (4, 2) across counter row 3; player talks from (4, 4) facing north.
//  BaldingMan wanders around (2, 6) (range 1x1), OldMan looks around at (6, 7).
constexpr std::string_view CELADON_PRIZE_ROOM_JSON = R"({
    "name": "CeladonCity_GameCorner_PrizeRoom",
    "source": {"decomp_file": "data/layouts/CeladonCity_GameCorner_PrizeRoom/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-22"},
    "allow_running": false,
    "tiles": [
        "#########",
        "#########",
        "#.......#",
        "#########",
        ".........",
        ".........",
        "##.....##",
        ".........",
        ".........",
        "#########"
    ],
    "elevation": [
        "000000000",
        "000000000",
        "033333330",
        "000000000",
        "333333333",
        "333333333",
        "003333300",
        "333333333",
        "333333333",
        "000000000"
    ],
    "points": {"warp_0": [3, 9], "warp_1": [4, 8], "warp_2": [5, 9], "prize_mon_front": [4, 4]},
    "warps": [
        {"x": 4, "y": 8, "enter": "walk_south", "to_map": "CeladonCity", "to_point": "warp_5"}
    ],
    "objects": [
        {"name": "balding_man", "kind": "npc", "x": 2, "y": 6, "wander": {"x0": 1, "y0": 5, "x1": 3, "y1": 7}},
        {"name": "old_man", "kind": "npc", "x": 6, "y": 7},
        {"name": "prize_clerk_mons", "kind": "npc", "x": 4, "y": 2},
        {"name": "prize_clerk_tms", "kind": "npc", "x": 6, "y": 2},
        {"name": "prize_clerk_items", "kind": "npc", "x": 2, "y": 2}
    ]
})";

//  CeladonCity_DepartmentStore_4F (13x15, LAYOUT_CELADON_CITY_DEPARTMENT_STORE_4F).
//  Elevator door at (6, 1); player lands at (6, 2) via lift_floor.
//  Clerk at (3, 13) faces north across counter row 12; player talks from (3, 11) facing south.
constexpr std::string_view CELADON_DEPT_4F_JSON = R"({
    "name": "CeladonCity_DepartmentStore_4F",
    "source": {"decomp_file": "data/layouts/CeladonCity_DepartmentStore_4F/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-22"},
    "allow_running": false,
    "tiles": [
        "#############",
        "######D#S####",
        "###.......###",
        "###.......###",
        "###.......###",
        "..........###",
        ".............",
        ".##..###.###.",
        ".##..........",
        ".##..###.###.",
        ".............",
        ".............",
        "########.....",
        ".......#.....",
        ".......#.##.."
    ],
    "elevation": [
        "0000000000000",
        "0000000000000",
        "0003333333000",
        "0003333333000",
        "0003333333000",
        "3333333333000",
        "3333333333333",
        "3003300030003",
        "3003333333333",
        "3003300030003",
        "3333333333333",
        "3333333333333",
        "0000000033333",
        "3333333033333",
        "3333333030033"
    ],
    "points": {"warp_0": [6, 1], "lift_front": [6, 2], "clerk_front": [3, 11]},
    "warps": [
        {"x": 6, "y": 1, "enter": "walk_into", "to_map": "CeladonCity_DepartmentStore_Elevator", "to_point": "warp_0"}
    ],
    "objects": [
        {"name": "man", "kind": "npc", "x": 6, "y": 6},
        {"name": "youngster", "kind": "npc", "x": 10, "y": 11, "wander": {"x0": 9, "y0": 10, "x1": 11, "y1": 12}},
        {"name": "clerk", "kind": "npc", "x": 3, "y": 13}
    ]
})";

//  VermilionCity_House2 (11x9, LAYOUT_HOUSE1).
//  Warps (3..5, 7) return to VermilionCity warp_6 (19, 17).
//  Elyssa (Spearow -> CH'DING trade) stands at (4, 4); player talks from (4, 5) facing north.
constexpr std::string_view VERMILION_HOUSE2_JSON = R"({
    "name": "VermilionCity_House2",
    "source": {"decomp_file": "data/layouts/House1/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-22"},
    "allow_running": false,
    "tiles": [
        "###########",
        "###########",
        "...........",
        "...........",
        ".....##....",
        ".....##....",
        "...........",
        "#.........#",
        "###########"
    ],
    "elevation": [
        "00000000000",
        "00000000000",
        "33333333333",
        "33333333333",
        "33333003333",
        "33333003333",
        "33333333333",
        "03333333330",
        "00000000000"
    ],
    "points": {"warp_0": [3, 7], "warp_1": [4, 7], "warp_2": [5, 7], "elyssa_front": [4, 5]},
    "warps": [
        {"x": 3, "y": 7, "enter": "walk_into", "to_map": "VermilionCity", "to_point": "warp_6"},
        {"x": 4, "y": 7, "enter": "walk_south", "to_map": "VermilionCity", "to_point": "warp_6"},
        {"x": 5, "y": 7, "enter": "walk_into", "to_map": "VermilionCity", "to_point": "warp_6"}
    ],
    "objects": [
        {"name": "elyssa", "kind": "npc", "x": 4, "y": 4}
    ]
})";

//  Route2_House (11x9, LAYOUT_HOUSE2).
//  Warps (3..5, 7) return to Route2 warp_4 (17, 22).
//  Reyley (Abra -> MIMIEN trade) stands at (7, 2); player talks from (7, 3) facing north.
constexpr std::string_view ROUTE2_HOUSE_JSON = R"({
    "name": "Route2_House",
    "source": {"decomp_file": "data/layouts/House2/map.bin", "revision": "ce6406d8c2eaf46d543c8a2606de0c3cb1951982", "date": "2026-11-22"},
    "allow_running": false,
    "tiles": [
        "###########",
        "###########",
        "...........",
        "...........",
        ".....##....",
        ".....##....",
        "...........",
        "#.........#",
        "###########"
    ],
    "elevation": [
        "00000000000",
        "00000000000",
        "33333333333",
        "33333333333",
        "33333003333",
        "33333003333",
        "33333333333",
        "03333333330",
        "33300033333"
    ],
    "points": {"warp_0": [3, 7], "warp_1": [4, 7], "warp_2": [5, 7], "reyley_front": [7, 3]},
    "warps": [
        {"x": 3, "y": 7, "enter": "walk_into", "to_map": "Route2", "to_point": "warp_4"},
        {"x": 4, "y": 7, "enter": "walk_south", "to_map": "Route2", "to_point": "warp_4"},
        {"x": 5, "y": 7, "enter": "walk_into", "to_map": "Route2", "to_point": "warp_4"}
    ],
    "objects": [
        {"name": "scientist", "kind": "npc", "x": 4, "y": 5},
        {"name": "reyley", "kind": "npc", "x": 7, "y": 2}
    ]
})";

void add_room_if_missing(std::vector<MapData>& maps, std::string_view json, std::string_view source){
    MapData room = parse_map(std::string(json), std::string(source));
    const bool present = std::any_of(maps.begin(), maps.end(), [&](const MapData& map){
        return map.name == room.name;
    });
    if (!present){
        maps.push_back(std::move(room));
    }
}

} // namespace


void add_mail_glitch_rooms(std::vector<MapData>& maps){
    add_room_if_missing(maps, CELADON_RESTAURANT_JSON, "CeladonCity_Restaurant");
    add_room_if_missing(maps, CELADON_PRIZE_ROOM_JSON, "CeladonCity_GameCorner_PrizeRoom");
    add_room_if_missing(maps, CELADON_DEPT_4F_JSON, "CeladonCity_DepartmentStore_4F");
    add_room_if_missing(maps, VERMILION_HOUSE2_JSON, "VermilionCity_House2");
    add_room_if_missing(maps, ROUTE2_HOUSE_JSON, "Route2_House");
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
