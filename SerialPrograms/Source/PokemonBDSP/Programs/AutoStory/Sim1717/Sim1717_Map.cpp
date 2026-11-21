/*  BDSP AutoStory PR #1717 Map & World Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <stdexcept>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Sim1717_Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

SimTileKind glyph_to_kind(char c){
    switch (c){
    case '#': return SimTileKind::WALL;
    case '.': return SimTileKind::OPEN;
    case '~': return SimTileKind::TALL_GRASS;
    case 'w': return SimTileKind::WATER;
    case '^': return SimTileKind::LEDGE_NORTH;
    case 'v': return SimTileKind::LEDGE_SOUTH;
    case '<': return SimTileKind::LEDGE_WEST;
    case '>': return SimTileKind::LEDGE_EAST;
    case 'D': return SimTileKind::DOOR;
    case 'S': return SimTileKind::SIGN;
    case ' ': return SimTileKind::OUTSIDE;
    case '8': return SimTileKind::CLOSED_NORTH;
    case '2': return SimTileKind::CLOSED_SOUTH;
    case '4': return SimTileKind::CLOSED_WEST;
    case '6': return SimTileKind::CLOSED_EAST;
    default:  return SimTileKind::WALL;
    }
}

SimTile read_coord_array(const JsonArray& arr){
    return {
        (int)arr[0].to_integer_default(),
        (int)arr[1].to_integer_default()
    };
}

SimMapData make_rect_room(const std::string& name, int w, int h, SimTile door_tile){
    SimMapData m;
    m.name = name;
    m.width = w;
    m.height = h;
    m.indoor = true;
    m.allow_run = true;
    m.cells.assign(size_t(w) * size_t(h), SimTileKind::OPEN);
    for (int x = 0; x < w; x++){
        m.set({x, 0}, SimTileKind::WALL);
        m.set({x, h - 1}, SimTileKind::WALL);
    }
    for (int y = 0; y < h; y++){
        m.set({0, y}, SimTileKind::WALL);
        m.set({w - 1, y}, SimTileKind::WALL);
    }
    m.set(door_tile, SimTileKind::DOOR);
    return m;
}

} // namespace

Sim1717MapDB::Sim1717MapDB(const std::string& runtime_maps_dir){
    //  22 verified runtime maps from Beginning -> Floaroma.
    //  In BDSP, Running Shoes work both outdoors and indoors once obtained.
    load_runtime_map(runtime_maps_dir, "PlayerHouse2F",    true,  false, true);
    load_runtime_map(runtime_maps_dir, "PlayerHouse1F",    true,  false, true);
    load_runtime_map(runtime_maps_dir, "TwinleafTown",     false, false, true);
    load_runtime_map(runtime_maps_dir, "RivalHouse1F",     true,  false, true);
    load_runtime_map(runtime_maps_dir, "RivalHouse2F",     true,  false, true);
    load_runtime_map(runtime_maps_dir, "Route201",         false, false, true);
    load_runtime_map(runtime_maps_dir, "VerityLakefront",  false, false, true);
    load_runtime_map(runtime_maps_dir, "LakeVerity",       false, false, true);
    load_runtime_map(runtime_maps_dir, "SandgemTown",      false, false, true);
    load_runtime_map(runtime_maps_dir, "RowansLab",        true,  false, true);
    load_runtime_map(runtime_maps_dir, "Route202",         false, false, true);
    load_runtime_map(runtime_maps_dir, "JubilifeCity",     false, false, true);
    load_runtime_map(runtime_maps_dir, "Route203",         false, false, true);
    load_runtime_map(runtime_maps_dir, "OreburghGate",     true,  true,  true);
    load_runtime_map(runtime_maps_dir, "OreburghCity",     false, false, true);
    load_runtime_map(runtime_maps_dir, "OreburghMineB1F",  true,  true,  true);
    load_runtime_map(runtime_maps_dir, "OreburghMine",     true,  true,  true);
    load_runtime_map(runtime_maps_dir, "OreburghGym",      true,  false, true);
    load_runtime_map(runtime_maps_dir, "Route204South",    false, false, true);
    load_runtime_map(runtime_maps_dir, "RavagedPath",      true,  true,  true);
    load_runtime_map(runtime_maps_dir, "Route204North",    false, false, true);
    load_runtime_map(runtime_maps_dir, "FloaromaTown",     false, false, true);

    build_synthetic_interiors();
    build_overworld_offsets();
    build_warps();
    build_trainers();
    build_landmarks();
}

void Sim1717MapDB::load_runtime_map(
    const std::string& dir, const std::string& name, bool indoor, bool cave, bool allow_run
){
    const std::string path = dir + "/" + name + ".json";
    std::string text = file_to_string(path);
    //  Strip UTF-8 BOM if present.
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF){
        text.erase(0, 3);
    }
    const JsonValue root = parse_json(text);
    const JsonObject* obj = root.to_object();
    if (!obj){
        throw std::runtime_error("Sim1717MapDB: failed to parse " + path);
    }

    SimMapData m;
    m.name = name;
    m.indoor = indoor;
    m.cave = cave;
    m.allow_run = allow_run;

    const JsonArray* rows = obj->get_array("tiles");
    if (!rows || rows->empty()){
        throw std::runtime_error("Sim1717MapDB: missing tiles in " + path);
    }
    m.height = (int)rows->size();
    const std::string* first_row = (*rows)[0].to_string();
    if (!first_row){
        throw std::runtime_error("Sim1717MapDB: invalid tile row in " + path);
    }
    m.width = (int)first_row->size();
    m.cells.reserve(size_t(m.width) * size_t(m.height));
    for (size_t y = 0; y < rows->size(); y++){
        const std::string* line = (*rows)[y].to_string();
        if (!line || (int)line->size() != m.width){
            throw std::runtime_error("Sim1717MapDB: ragged tile row in " + path);
        }
        for (char c : *line){
            m.cells.push_back(glyph_to_kind(c));
        }
    }

    if (const JsonObject* pts = obj->get_object("points")){
        for (const auto& [k, v] : *pts){
            if (const JsonArray* arr = v.to_array(); arr && arr->size() >= 2){
                m.points[k] = read_coord_array(*arr);
            }
        }
    }

    if (const JsonArray* npcs = obj->get_array("npcs")){
        for (size_t i = 0; i < npcs->size(); i++){
            const JsonObject* n = (*npcs)[i].to_object();
            if (!n){
                continue;
            }
            SimNpc npc;
            if (const std::string* s = n->get_string("name")){
                npc.name = *s;
            }
            if (const JsonArray* c = n->get_array("coord"); c && c->size() >= 2){
                npc.pos = read_coord_array(*c);
            }
            if (const JsonObject* w = n->get_object("wander")){
                const JsonArray* lo = w->get_array("min");
                const JsonArray* hi = w->get_array("max");
                if (lo && hi && lo->size() >= 2 && hi->size() >= 2){
                    SimTile t0 = read_coord_array(*lo);
                    SimTile t1 = read_coord_array(*hi);
                    npc.wander = SimWanderBox{t0.x, t0.y, t1.x, t1.y};
                }
            }
            m.npcs.push_back(std::move(npc));
        }
    }

    if (const JsonArray* ces = obj->get_array("coord_events")){
        for (size_t i = 0; i < ces->size(); i++){
            const JsonObject* e = (*ces)[i].to_object();
            if (!e){
                continue;
            }
            SimCoordEvent ce;
            ce.map = name;
            if (const std::string* s = e->get_string("name")){
                ce.name = *s;
            }
            if (const std::string* s = e->get_string("var")){
                ce.var = *s;
            }
            ce.value = (int)e->get_integer_default("value", 0);
            SimTile origin{0, 0};
            if (const JsonArray* c = e->get_array("coord"); c && c->size() >= 2){
                origin = read_coord_array(*c);
            }else{
                origin.x = (int)e->get_integer_default("x", 0);
                origin.y = (int)e->get_integer_default("y", 0);
            }
            const int w = std::max(1, (int)e->get_integer_default("width", 1));
            const int l = std::max(1, (int)e->get_integer_default("length", 1));
            for (int ry = origin.y; ry < origin.y + l; ry++){
                for (int rx = origin.x; rx < origin.x + w; rx++){
                    ce.tiles.push_back({rx, ry});
                }
            }
            m.coord_events.push_back(std::move(ce));
        }
    }

    if (const JsonArray* items = obj->get_array("items")){
        for (size_t i = 0; i < items->size(); i++){
            const JsonObject* it = (*items)[i].to_object();
            if (!it){
                continue;
            }
            SimItem item;
            if (const std::string* s = it->get_string("item")){
                item.item = *s;
            }
            if (const JsonArray* c = it->get_array("coord"); c && c->size() >= 2){
                item.pos = read_coord_array(*c);
            }
            m.items.push_back(std::move(item));
        }
    }

    //  Console-verified fix: Route201 Potion clerk (school_kid_m_9) is at point potion_giver (43, 3).
    if (name == "Route201"){
        m.npcs.push_back({"potion_clerk", {43, 3}, std::nullopt});
    }

    m_maps.emplace(name, std::move(m));
}

void Sim1717MapDB::build_synthetic_interiors(){
    //  TrainersSchool: small room, door at (6, 10), Barry standing at (5, 4)
    //  (matches Segment_00: north 2000ms to top wall, left 200ms, A to talk to Barry, south 1521ms out).
    {
        SimMapData m = make_rect_room("TrainersSchool", 13, 11, {6, 10});
        //  Top wall at y=2 so ~8 tiles north from (6, 9) hits the wall cleanly.
        for (int x = 0; x < 13; x++){
            m.set({x, 2}, SimTileKind::WALL);
        }
        m.npcs.push_back({"barry_school", {5, 3}, std::nullopt});
        m_maps.emplace(m.name, std::move(m));
    }

    //  JubilifeCenter: standard BDSP Pokemon Center interior.
    //  Door at (7, 11), PC at (11, 4), nurse counter at (7, 3).
    //  Matches Segment_00 end: north 1099ms (~4-5 tiles), 4 D-pad right, north 642ms to PC;
    //  and Segment_01 start: south 1465ms, left 644ms, leave_pokemon_center_before_barry_1.
    {
        SimMapData m = make_rect_room("JubilifeCenter", 15, 12, {7, 11});
        for (int x = 1; x < 14; x++){
            m.set({x, 2}, SimTileKind::WALL);
            m.set({x, 3}, SimTileKind::WALL);
        }
        //  Open counter in front of PC at (11, 4) and center aisle.
        m_maps.emplace(m.name, std::move(m));
    }

    //  OreburghMart: small Poke Mart interior. Door at (4, 9), clerk counter north.
    {
        SimMapData m = make_rect_room("OreburghMart", 11, 10, {4, 9});
        for (int x = 1; x < 10; x++){
            m.set({x, 4}, SimTileKind::WALL);
        }
        m.npcs.push_back({"mart_clerk", {4, 4}, std::nullopt});
        m_maps.emplace(m.name, std::move(m));
    }

    //  FloaromaCenter: standard Pokemon Center interior.
    {
        SimMapData m = make_rect_room("FloaromaCenter", 15, 12, {7, 11});
        for (int x = 1; x < 14; x++){
            m.set({x, 3}, SimTileKind::WALL);
        }
        m_maps.emplace(m.name, std::move(m));
    }
}

void Sim1717MapDB::build_overworld_offsets(){
    //  Exact Diamond/BDSP world offsets from StoryPlaces.cpp.
    m_overworld_offsets = {
        {"TwinleafTown",    {96,  864}},
        {"Route201",        {96,  839}},
        {"VerityLakefront", {64,  832}},
        {"SandgemTown",     {160, 832}},
        {"Route202",        {159, 800}},
        {"JubilifeCity",    {128, 736}},
        {"Route203",        {191, 740}},
        {"Route204South",   {160, 704}},
        {"Route204North",   {160, 672}},
        {"FloaromaTown",    {160, 640}},
    };
}

void Sim1717MapDB::build_warps(){
    auto add = [&](
        const std::string& fm, SimTile ft, std::optional<SimDir> req,
        const std::string& tm, SimTile tt, SimDir tf, bool white = false
    ){
        m_warps.push_back({fm, ft, req, tm, tt, tf, white});
    };

    //  PlayerHouse2F <-> PlayerHouse1F (stairs at east of (8,4) -> (9,4))
    add("PlayerHouse2F", {9, 4}, SimDir::EAST,  "PlayerHouse1F", {10, 3}, SimDir::SOUTH, false);
    add("PlayerHouse1F", {6, 10}, SimDir::SOUTH, "TwinleafTown", {20, 22}, SimDir::SOUTH, false);
    add("TwinleafTown",  {20, 21}, SimDir::NORTH, "PlayerHouse1F", {6, 9}, SimDir::NORTH, false);

    //  TwinleafTown <-> RivalHouse1F <-> RivalHouse2F
    add("TwinleafTown",  {9, 11}, SimDir::NORTH, "RivalHouse1F", {6, 9}, SimDir::NORTH, false);
    add("RivalHouse1F",  {6, 10}, SimDir::SOUTH, "TwinleafTown", {9, 12}, SimDir::SOUTH, false);
    //  RivalHouse1F stairs: top-left corner stairs entered going east from (2,3) -> (3,3) or up to (2,2)/(3,2).
    //  Let's check RivalHouse1F stairs from StoryTable: east from stairs point -> RivalHouse2F (3, 4).
    if (has_map("RivalHouse1F")){
        const auto& rh1 = map("RivalHouse1F");
        if (auto it = rh1.points.find("stairs"); it != rh1.points.end()){
            add("RivalHouse1F", it->second.next(SimDir::EAST), SimDir::EAST, "RivalHouse2F", {3, 4}, SimDir::SOUTH, false);
        }
    }
    if (has_map("RivalHouse2F")){
        const auto& rh2 = map("RivalHouse2F");
        if (auto it = rh2.points.find("stairs"); it != rh2.points.end()){
            add("RivalHouse2F", it->second.next(SimDir::WEST), SimDir::WEST, "RivalHouse1F", {2, 3}, SimDir::SOUTH, false);
        }
    }

    //  RowansLab <-> SandgemTown
    add("RowansLab",   {7, 15}, SimDir::SOUTH, "SandgemTown", {8, 11}, SimDir::SOUTH, false);
    add("SandgemTown", {8, 10}, SimDir::NORTH, "RowansLab",   {7, 14}, SimDir::NORTH, false);

    //  JubilifeCity <-> TrainersSchool & JubilifeCenter
    add("JubilifeCity",   {40, 40}, SimDir::NORTH, "TrainersSchool", {6, 9},  SimDir::NORTH, false);
    add("TrainersSchool", {6, 10},  SimDir::SOUTH, "JubilifeCity",   {40, 41}, SimDir::SOUTH, false);
    add("JubilifeCity",   {52, 40}, SimDir::NORTH, "JubilifeCenter", {7, 10}, SimDir::NORTH, false);
    add("JubilifeCenter", {7, 11},  SimDir::SOUTH, "JubilifeCity",   {52, 41}, SimDir::SOUTH, false);

    //  Route203 <-> OreburghGate <-> OreburghCity
    add("Route203",     {56, 9},  SimDir::EAST, "OreburghGate", {4, 22},  SimDir::EAST, false);
    add("OreburghGate", {3, 22},  SimDir::WEST, "Route203",     {55, 9},  SimDir::WEST, true);
    add("OreburghGate", {28, 22}, SimDir::EAST, "OreburghCity", {2, 13},  SimDir::EAST, true);
    add("OreburghCity", {1, 13},  SimDir::WEST, "OreburghGate", {27, 22}, SimDir::WEST, false);

    //  OreburghCity <-> OreburghMineB1F <-> OreburghMine
    for (int x = 44; x <= 48; x++){
        add("OreburghCity", {x, 59}, SimDir::SOUTH, "OreburghMineB1F", {12, 2}, SimDir::SOUTH, false);
    }
    for (int x = 11; x <= 13; x++){
        add("OreburghMineB1F", {x, 1},  SimDir::NORTH, "OreburghCity", {46, 58}, SimDir::NORTH, true);
        add("OreburghMineB1F", {x, 21}, SimDir::SOUTH, "OreburghMine", {15, 2},  SimDir::SOUTH, false);
    }
    add("OreburghMineB1F", {12, 22}, SimDir::SOUTH, "OreburghMine", {15, 2}, SimDir::SOUTH, false);
    add("OreburghMine",    {15, 1},  SimDir::NORTH, "OreburghMineB1F", {12, 20}, SimDir::NORTH, false);

    //  OreburghCity <-> OreburghGym & OreburghMart
    add("OreburghCity", {26, 20}, SimDir::NORTH, "OreburghGym",  {5, 24},  SimDir::NORTH, false);
    add("OreburghGym",  {5, 24},  SimDir::SOUTH, "OreburghCity", {26, 21}, SimDir::SOUTH, false);
    add("OreburghCity", {29, 10}, SimDir::NORTH, "OreburghMart", {4, 8},   SimDir::NORTH, false);
    add("OreburghMart", {4, 9},   SimDir::SOUTH, "OreburghCity", {29, 11}, SimDir::SOUTH, false);

    //  Route204South <-> RavagedPath <-> Route204North
    add("Route204South", {11, 1},  SimDir::NORTH, "RavagedPath",   {19, 50}, SimDir::NORTH, false);
    add("RavagedPath",   {19, 51}, SimDir::SOUTH, "Route204South", {11, 2},  SimDir::SOUTH, true);
    add("RavagedPath",   {28, 45}, SimDir::SOUTH, "Route204North", {20, 27}, SimDir::SOUTH, true);

    //  FloaromaTown <-> FloaromaCenter
    add("FloaromaTown",   {16, 26}, SimDir::NORTH, "FloaromaCenter", {7, 10},  SimDir::NORTH, false);
    add("FloaromaCenter", {7, 11},  SimDir::SOUTH, "FloaromaTown",   {16, 27}, SimDir::SOUTH, false);
}

void Sim1717MapDB::build_trainers(){
    auto npc_pos = [&](const std::string& map_name, const std::string& npc_name) -> SimTile{
        const auto& m = map(map_name);
        for (const auto& n : m.npcs){
            if (n.name == npc_name){
                return n.pos;
            }
        }
        if (auto it = m.points.find(npc_name); it != m.points.end()){
            return it->second;
        }
        throw std::runtime_error("Sim1717MapDB: missing trainer " + npc_name + " in " + map_name);
    };

    //  Route 202 (from StoryMaps.cpp)
    m_trainers.push_back({"Tristan", "Route202", npc_pos("Route202", "trainer_tristan"), SimDir::SOUTH, 5, false});
    m_trainers.push_back({"Natalie", "Route202", npc_pos("Route202", "trainer_natalie"), SimDir::SOUTH, 4, false});
    m_trainers.push_back({"Logan",   "Route202", npc_pos("Route202", "trainer_logan"),   SimDir::SOUTH, 5, false});

    //  Route 203 (from StoryPlaces.cpp)
    //  Note: youngster_1 (Dallas) looks south/west on row 17? Wait: let's match exact StoryPlaces.cpp!
    //  In Route203: youngster_1=(13,10) WEST 2, lass_2=(43,17) WEST 5, youngster_3=(31,16) SOUTH 4, lass_4=(49,8) SOUTH 5.
    //  Also check Dallas/Sebastian in Butters' Segment 01: Dallas is the first Route 203 trainer eastward (youngster_3 or youngster on south path).
    m_trainers.push_back({"Route203 Youngster West", "Route203", npc_pos("Route203", "youngster_1"), SimDir::WEST,  2, false});
    m_trainers.push_back({"Route203 Dallas/Youngster", "Route203", npc_pos("Route203", "youngster_3"), SimDir::SOUTH, 4, false});
    m_trainers.push_back({"Route203 Sebastian/Lass",   "Route203", npc_pos("Route203", "lass_2"),      SimDir::WEST,  5, true});
    m_trainers.push_back({"Route203 Lass Gate",        "Route203", npc_pos("Route203", "lass_4"),      SimDir::SOUTH, 5, false});

    //  Oreburgh Gate (from StoryPlaces.cpp)
    m_trainers.push_back({"Camper Curtis",   "OreburghGate", npc_pos("OreburghGate", "camper_1"),    SimDir::NORTH, 4, false});
    m_trainers.push_back({"Picnicker Diana", "OreburghGate", npc_pos("OreburghGate", "picnicker_0"), SimDir::SOUTH, 2, false});

    //  Oreburgh Gym (from StoryMaps.cpp)
    m_trainers.push_back({"Gym Youngster 1", "OreburghGym", npc_pos("OreburghGym", "trainer_first_tier"),  SimDir::EAST, 3, false});
    m_trainers.push_back({"Gym Youngster 2", "OreburghGym", npc_pos("OreburghGym", "trainer_second_tier"), SimDir::WEST, 4, false});

    //  Route 204 South (from StoryMaps.cpp)
    m_trainers.push_back({"Lass Samantha",   "Route204South", npc_pos("Route204South", "trainer_samantha"), SimDir::SOUTH, 4, false});
    m_trainers.push_back({"Lass Sarah",      "Route204South", npc_pos("Route204South", "trainer_sarah"),    SimDir::NORTH, 4, false});
    m_trainers.push_back({"Lass Sarah",      "Route204South", npc_pos("Route204South", "trainer_sarah"),    SimDir::EAST,  4, false});
    m_trainers.push_back({"Youngster Tyler", "Route204South", npc_pos("Route204South", "trainer_tyler"),    SimDir::EAST,  4, true});
    m_trainers.push_back({"Youngster Tyler", "Route204South", npc_pos("Route204South", "trainer_tyler"),    SimDir::WEST,  4, true});

    //  Route 204 North (from StoryMaps.cpp)
    m_trainers.push_back({"Twins Left",          "Route204North", npc_pos("Route204North", "trainer_twin_left"),  SimDir::SOUTH, 3, false});
    m_trainers.push_back({"Twins Right",         "Route204North", npc_pos("Route204North", "trainer_twin_right"), SimDir::SOUTH, 4, false});
    m_trainers.push_back({"Aroma Lady Teighler", "Route204North", npc_pos("Route204North", "trainer_taylor"),     SimDir::SOUTH, 4, false});
    m_trainers.push_back({"Aroma Lady Teighler", "Route204North", npc_pos("Route204North", "trainer_taylor"),     SimDir::EAST,  4, false});
    m_trainers.push_back({"Bug Catcher Brandon", "Route204North", npc_pos("Route204North", "trainer_brandon"),    SimDir::WEST,  4, true});
    m_trainers.push_back({"Bug Catcher Brandon", "Route204North", npc_pos("Route204North", "trainer_brandon"),    SimDir::EAST,  4, true});
}

void Sim1717MapDB::build_landmarks(){
    auto add = [&](SimLandmarkDef def){
        std::string key = def.matcher_name;
        m_landmarks.emplace(std::move(key), std::move(def));
    };

    //  Segment 00: Jubilife Trainers' School sign (door at (40, 40), stand at (40, 41)).
    add({"match_jubilife_trainers_school_sign", "JubilifeCity", 40.0, 41.0, 5.0, 4.0, 6.0, 5.0, false, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 00: Poketch president at (46, 35); Segment_00 talks facing DOWN from (46, 34).
    add({"match_poketch_landmarks", "JubilifeCity", 46.0, 34.0, 4.5, 4.0, 7.5, 6.5, true, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 00 & 02: Jubilife Pokemon Center entrance (door at (52, 40), stand at (52, 41)).
    add({"match_jubilife_center_entrance", "JubilifeCity", 52.0, 41.0, 5.0, 4.0, 6.0, 5.0, false, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 01: Oreburgh Gym entrance (door at (26, 20), stand at (26, 21)).
    add({"match_oreburgh_gym_entrance", "OreburghCity", 26.0, 21.0, 4.5, 3.5, 6.0, 5.0, false, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 02: Oreburgh Mart entrance (door at (29, 10), stand at (29, 11)).
    add({"match_oreburgh_mart", "OreburghCity", 29.0, 11.0, 4.5, 3.5, 6.0, 5.0, false, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 02: Jubilife street approach (intersection on north-south road x=46, y=22).
    add({"match_jubilife_street_approach", "JubilifeCity", 46.0, 22.0, 5.5, 4.0, 8.0, 5.5, true, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 02: Jubilife Galactic plaza approach (just south of Rowan trigger (45..47, 7), stand at (47, 9)).
    add({"match_jubilife_plaza_approach", "JubilifeCity", 47.0, 9.0, 5.0, 4.5, 7.5, 6.5, true, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});

    //  Segment 02: Ravaged Path south entrance (door at (11, 1), stand at (11, 2)).
    add({"match_ravaged_path_south_entrance", "Route204South", 11.0, 2.0, 4.5, 4.0, 6.0, 5.5, true, 1.0/64.0, 1.0/36.0, 0.075, 0.115, 2});

    //  Segment 02: Floaroma Pokemon Center entrance (door at (16, 26), stand at (16, 27)).
    add({"match_floaroma_center_entrance", "FloaromaTown", 16.0, 27.0, 5.5, 4.5, 7.0, 5.5, false, 1.0/50.0, 1.0/54.0, 0.075, 0.115, 2});
}

const SimMapData& Sim1717MapDB::map(const std::string& name) const{
    auto it = m_maps.find(name);
    if (it == m_maps.end()){
        throw std::runtime_error("Sim1717MapDB: unknown map " + name);
    }
    return it->second;
}

bool Sim1717MapDB::has_map(const std::string& name) const{
    return m_maps.contains(name);
}

std::optional<SimTile> Sim1717MapDB::overworld_offset(const std::string& map_name) const{
    auto it = m_overworld_offsets.find(map_name);
    if (it == m_overworld_offsets.end()){
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::pair<std::string, SimTile>> Sim1717MapDB::cross_border(
    const std::string& map_name, const SimTile& next_tile
) const{
    const auto off = overworld_offset(map_name);
    if (!off){
        return std::nullopt;
    }
    const SimTile world{off->x + next_tile.x, off->y + next_tile.y};
    for (const auto& [other_name, other_off] : m_overworld_offsets){
        if (other_name == map_name){
            continue;
        }
        const SimTile local{world.x - other_off.x, world.y - other_off.y};
        const SimMapData& other = map(other_name);
        if (other.contains(local) && other.at(local) != SimTileKind::OUTSIDE){
            return std::make_pair(other_name, local);
        }
    }
    return std::nullopt;
}

const SimWarpRule* Sim1717MapDB::find_warp(
    const std::string& map_name, const SimTile& tile, SimDir dir
) const{
    for (const auto& w : m_warps){
        if (w.from_map == map_name && w.from_tile == tile){
            if (!w.required_dir || *w.required_dir == dir){
                return &w;
            }
        }
    }
    return nullptr;
}

std::vector<SimTrainer> Sim1717MapDB::trainers_on(const std::string& map_name) const{
    std::vector<SimTrainer> out;
    for (const auto& t : m_trainers){
        if (t.map == map_name){
            out.push_back(t);
        }
    }
    return out;
}

const SimLandmarkDef* Sim1717MapDB::landmark(const std::string& matcher_name) const{
    auto it = m_landmarks.find(matcher_name);
    return it == m_landmarks.end() ? nullptr : &it->second;
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
