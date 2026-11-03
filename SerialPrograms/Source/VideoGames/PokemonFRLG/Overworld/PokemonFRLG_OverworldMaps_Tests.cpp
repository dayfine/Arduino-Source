/*  Overworld Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_OverworldMaps : public UnitTest{
public:
    Test_OverworldMaps(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldMaps - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};


const char* const VALID_MAP = R"({
    "name": "Test",
    "source": {"decomp_file": "data/maps/Test/map.json", "revision": "test", "date": "2026-10-31"},
    "tiles": ["#.w", "~D."],
    "elevation": ["031", "3f3"],
    "points": {"door": [1, 1]},
    "warps": [
        {"x": 1, "y": 1, "enter": "walk_into", "to_map": "Other", "to_point": "warp_2"},
        {"x": 2, "y": 1, "enter": "walk_south"}
    ],
    "objects": [{"name": "nurse", "kind": "npc", "x": 0, "y": 1, "wander": {"x0": 0, "y0": 1, "x1": 1, "y1": 1}, "hide_flag": "FLAG_X"}],
    "flag_sets": {"later": ["FLAG_X", "FLAG_Y"]},
    "connections": [{"side": "south", "to_map": "Other", "offset": -4}]
})";

//  The ParseException that parsing `json` throws, or empty if it doesn't.
std::string parse_error(const std::string& json){
    try{
        parse_map(json, "Test.json");
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

//  Whether `text` contains every one of `parts`.
bool mentions(const std::string& text, std::initializer_list<const char*> parts){
    for (const char* part : parts){
        if (text.find(part) == std::string::npos){
            return false;
        }
    }
    return true;
}

UnitTestResult test_valid_map(){
    const MapData map = parse_map(VALID_MAP, "Test.json");
    if (map.name != "Test" || map.tiles.width != 3 || map.tiles.height != 2
        || map.tiles.at({0, 1}) != TileKind::TALL_GRASS || exit_mat(map, {2, 1}) != Direction::SOUTH
    ){
        return "Wrong name, size or tiles.";
    }
    if (elevation_at(map, {2, 0}) != 1 || elevation_at(map, {1, 1}) != 15){
        return "Wrong elevation.";
    }
    const MapWarp* door = warp_at(map, {1, 1});
    if (map.warps.size() != 2 || door == nullptr || !door->to || door->to->map != "Other" || door->to->point != "warp_2"){
        return "Wrong warps.";
    }
    const MapObject* nurse = object_at(map, {1, 1});
    if (map.objects.size() != 1 || nurse == nullptr || nurse->name != "nurse" || nurse->hide_flag != "FLAG_X"){
        return "Wrong objects.";
    }
    const MapConnection& connection = map.connections.at(0);
    if (connection.side != Direction::SOUTH || connection.offset != -4 || connection.to_map != "Other"){
        return "Wrong connections.";
    }
    if (map.points.at("door") != TilePos{1, 1} || map.flag_sets.at("later") != std::set<std::string>{"FLAG_X", "FLAG_Y"}){
        return "Wrong points or flag sets.";
    }
    return true;
}

UnitTestResult test_missing_field(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"tiles\""), 7, "\"cells\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles", "missing"})){
        return "Expected an error naming the missing tiles, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_elevation_left_out(){
    std::string json = VALID_MAP;
    json.erase(json.find("\"elevation\""), std::string("\"elevation\": [\"031\", \"3f3\"],").size());
    const MapData map = parse_map(json, "Test.json");
    for (int y = 0; y < 2; y++){
        for (int x = 0; x < 3; x++){
            if (elevation_at(map, {x, y}) != ELEVATION_GROUND){
                return "Expected ground everywhere without an elevation grid.";
            }
        }
    }
    return true;
}

UnitTestResult test_bad_field_type(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"x\": 1"), 6, "\"x\": \"1\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "warps[0]", "x", "integer"})){
        return "Expected an error naming warps[0].x, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_bad_row_length(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"~D.\""), 5, "\"~D\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles[1]", "string of 3 glyphs"})){
        return "Expected an error naming the short row, got: \"" + error + "\"";
    }
    return true;
}

UnitTestResult test_unknown_tile(){
    std::string json = VALID_MAP;
    json.replace(json.find("\"#.w\""), 5, "\"#.?\"");
    const std::string error = parse_error(json);
    if (!mentions(error, {"Test.json", "tiles[0]", "unknown glyph '?'", "(2, 0)"})){
        return "Expected an error naming the unknown tile, got: \"" + error + "\"";
    }
    return true;
}

//  Route 3's Lass Robin, shrunk: she wanders x 2, y 2..4 looking north and
//  south, sight 3, below a corridor (row 1) walled off at x 2 above it; a
//  bug catcher at (5, 4) looks west, sight 2.
const char* const TRAINERS_MAP = R"({
    "name": "Trainers",
    "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["##.####", ".......", ".......", ".......", ".......", "~~~~~~~"],
    "objects": [
        {"name": "lass", "kind": "npc", "x": 2, "y": 3, "wander": {"x0": 2, "y0": 2, "x1": 2, "y1": 4}, "trainer": {"sight": 3, "looks": ["north", "south"]}},
        {"name": "bug_catcher", "kind": "npc", "x": 5, "y": 4, "facing": "west", "trainer": {"sight": 2}},
        {"name": "youngster", "kind": "npc", "x": 6, "y": 1}
    ]
})";

//  A wanderer sees from every tile she may stand on, short of a wall; a
//  trainer standing still only along his facing; a non-trainer nothing.
UnitTestResult test_trainer_sight(){
    const MapData map = parse_map(TRAINERS_MAP, "Trainers.json");
    const MapObject& lass = map.objects[0];
    const MapObject& bug_catcher = map.objects[1];
    const MapObject& youngster = map.objects[2];
    struct Case{
        const MapObject* object;
        TilePos tile;
        bool sees;
    };
    const Case cases[] = {
        {&lass, {2, 1}, true},          //  from (2, 2..4), north
        {&lass, {2, 0}, true},          //  the gap in the wall, 2 tiles up from (2, 2)
        {&lass, {2, 5}, true},          //  south, from (2, 4)
        {&lass, {1, 1}, false},
        {&bug_catcher, {3, 4}, true},
        {&bug_catcher, {2, 4}, false},  //  past his sight
        {&bug_catcher, {5, 3}, false},  //  not his way
        {&youngster, {5, 1}, false},
    };
    for (const Case& c : cases){
        if (trainer_may_see(map, *c.object, c.tile) != c.sees){
            return c.object->name + (c.sees ? " should see " : " shouldn't see ") + Pokemon::to_string(c.tile) + ".";
        }
    }
    return true;
}

UnitTestResult test_not_json(){
    const std::string error = parse_error("{\"name\": ");
    if (!mentions(error, {"Test.json", "not a JSON object"})){
        return "Expected an error for broken JSON, got: \"" + error + "\"";
    }
    return true;
}

const MapData* built_in(const std::vector<MapData>& maps, const std::string& name){
    for (const MapData& map : maps){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Route 3, Route 4 and its Pokemon Center load, joined to Pewter City; the
//  Center doesn't let the player run, the routes do.
UnitTestResult test_route3_route4_maps(){
    const std::vector<MapData> maps = overworld_maps();
    for (const char* name : {"Route3", "Route4", "Route4_PokemonCenter_1F"}){
        if (built_in(maps, name) == nullptr){
            return std::string("Missing map ") + name + ".";
        }
    }
    if (!built_in(maps, "Route3")->allow_running || !built_in(maps, "PewterCity")->allow_running
        || built_in(maps, "Route4_PokemonCenter_1F")->allow_running || built_in(maps, "PewterCity_PokemonCenter_1F")->allow_running
    ){
        return "Expected running on the routes and Pewter City, not in the Pokemon Centers.";
    }
    const MapPosition nurse{"Route4_PokemonCenter_1F", map_point(maps, "Route4_PokemonCenter_1F", "nurse_front")};
    const MapPosition salesman{"Route4_PokemonCenter_1F", map_point(maps, "Route4_PokemonCenter_1F", "salesman_front")};
    if (nurse.point != TilePos{7, 4} || salesman.point != TilePos{2, 3}){
        return "Unexpected nurse_front or salesman_front.";
    }
    const MapPosition pewter{"PewterCity", {45, 21}};
    if (!plan_overworld_route(maps, pewter, RouteGoal{nurse, Direction::NORTH})){
        return "No route from Pewter City over Route 3 to Route 4's Pokemon Center.";
    }
    return true;
}

//  Mt. Moon's three floors load, every floor tile of them with wild
//  encounters, and join Route 4's two cave mouths: the way from the west
//  mouth (19, 5) to the east one (32, 5) goes down to B2F and back up,
//  through the gap the fossils leave once taken, and comes out walked a
//  tile south, facing south.
UnitTestResult test_mt_moon_maps(){
    const std::vector<MapData> maps = overworld_maps({"FLAG_HIDE_DOME_FOSSIL", "FLAG_HIDE_HELIX_FOSSIL"});
    for (const char* name : {"MtMoon_1F", "MtMoon_B1F", "MtMoon_B2F"}){
        const MapData* map = built_in(maps, name);
        if (map == nullptr){
            return std::string("Missing map ") + name + ".";
        }
        for (int y = 0; y < map->tiles.height; y++){
            for (int x = 0; x < map->tiles.width; x++){
                const TilePos tile{x, y};
                if (map->tiles.at(tile) == TileKind::OPEN && warp_at(*map, tile) == nullptr){
                    return std::format("{}: open floor without wild encounters at {}.", name, Pokemon::to_string(tile));
                }
            }
        }
    }
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, MapPosition{"Route4", {19, 6}}, RouteGoal{MapPosition{"Route4", {32, 6}}, Direction::SOUTH}
    );
    if (!route || route->empty()){
        return "No route through Mt. Moon from Route 4's west mouth to its east one.";
    }
    std::set<std::string> floors;
    for (const RouteStep& step : *route){
        if (step.warps){
            floors.insert(step.end.map);
        }
    }
    if (floors != std::set<std::string>{"MtMoon_1F", "MtMoon_B1F", "MtMoon_B2F", "Route4"} || route->back().facing_after != Direction::SOUTH){
        return "Expected the way through to warp onto every floor and out to Route 4 facing south.";
    }
    return true;
}

//  Mt. Moon B2F's floor closed on its north side, along row 14, has wild
//  encounters like the cave floor south of it: a leg along it is wild, so the
//  walker watches for a battle there.
UnitTestResult test_mt_moon_closed_floor_wild(){
    const std::vector<MapData> maps = overworld_maps({"FLAG_HIDE_DOME_FOSSIL", "FLAG_HIDE_HELIX_FOSSIL"});
    const MapData* map = built_in(maps, "MtMoon_B2F");
    if (map == nullptr){
        return "Missing map MtMoon_B2F.";
    }
    if (map->tiles.at({40, 14}) != TileKind::CLOSED_NORTH || !is_tall_grass(*map, {40, 14})){
        return "Expected MtMoon_B2F (40, 14) to be floor closed on the north, with wild encounters.";
    }
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, MapPosition{"MtMoon_B2F", {37, 14}}, RouteGoal{MapPosition{"MtMoon_B2F", {46, 14}}, Direction::EAST}
    );
    if (!route || route->size() != 1){
        return "Expected one leg along MtMoon_B2F's row 14.";
    }
    const RouteStep& step = route->front();
    if (step.direction != Direction::EAST || step.tiles != 9 || !step.wild){
        return "Expected a wild leg 9 tiles east along row 14, got " + to_string(step) + ".";
    }
    return true;
}

//  Whether a route plans from Cerulean's Pokemon Center, at its nurse, to
//  `goal` on `maps`.
bool plans_from_cerulean_nurse(const std::vector<MapData>& maps, const RouteGoal& goal){
    const MapPosition nurse{"CeruleanCity_PokemonCenter_1F", map_point(maps, "CeruleanCity_PokemonCenter_1F", "nurse_front")};
    return plan_overworld_route(maps, nurse, goal, default_route_costs(), Direction::NORTH).has_value();
}

//  Cerulean City's Mart and Gym and Route 25's Sea Cottage load with the
//  points the story walks to, and join the city: from its Pokemon Center
//  a route plans to the Mart's clerk, to Misty, and north over Nugget
//  Bridge (Route 24) and east along Route 25 into Bill's Sea Cottage.
UnitTestResult test_cerulean_maps(){
    const std::vector<MapData> maps = overworld_maps();
    for (const char* name : {"CeruleanCity_Mart", "CeruleanCity_Gym", "Route25", "Route25_SeaCottage"}){
        if (built_in(maps, name) == nullptr){
            return std::string("Missing map ") + name + ".";
        }
    }
    const MapPosition clerk{"CeruleanCity_Mart", map_point(maps, "CeruleanCity_Mart", "clerk_front")};
    const MapPosition misty{"CeruleanCity_Gym", map_point(maps, "CeruleanCity_Gym", "misty_front")};
    if (clerk.point != TilePos{4, 3} || misty.point != TilePos{8, 7}){
        return "Unexpected clerk_front or misty_front.";
    }
    if (!plans_from_cerulean_nurse(maps, RouteGoal{clerk, Direction::WEST})){
        return "No route from Cerulean's Pokemon Center to the Mart's clerk.";
    }
    if (!plans_from_cerulean_nurse(maps, RouteGoal{misty, Direction::NORTH})){
        return "No route from Cerulean's Pokemon Center to Misty.";
    }
    const MapPosition bill_front{"Route25_SeaCottage", {7, 7}};
    if (!plans_from_cerulean_nurse(maps, RouteGoal{bill_front, Direction::NORTH})){
        return "No route from Cerulean's Pokemon Center over Nugget Bridge and Route 25 into Bill's Sea Cottage.";
    }
    return true;
}

//  Cerulean's burgled house, Route 5, the Underground Path, Route 6,
//  Vermilion City and the S.S. Anne load and join: from Cerulean's Pokemon
//  Center a route plans through the house, its back yard and the
//  Underground Path to Vermilion's nurse, and from there onto
//  the ship and up to the Captain. The rival a new game hides
//  (FLAG_HIDE_SS_ANNE_RIVAL, flag set "new_game") stands on the Captain's
//  door until then: with him there, no route reaches the Captain. With
//  Surf's cost below 0, no route surfs.
UnitTestResult test_vermilion_maps(){
    const std::vector<MapData> all = overworld_maps();
    for (const char* name : {
        "CeruleanCity_House2", "Route5", "UndergroundPath_NorthEntrance", "UndergroundPath_NorthSouthTunnel", "UndergroundPath_SouthEntrance",
        "Route6", "VermilionCity", "VermilionCity_PokemonCenter_1F",
        "SSAnne_Exterior", "SSAnne_1F_Corridor", "SSAnne_2F_Corridor", "SSAnne_CaptainsOffice",
    }){
        if (built_in(all, name) == nullptr){
            return std::string("Missing map ") + name + ".";
        }
    }
    const std::set<std::string> new_game = map_flag_set(all, "new_game");
    if (new_game.count("FLAG_HIDE_SS_ANNE_RIVAL") == 0){
        return "Expected FLAG_HIDE_SS_ANNE_RIVAL in flag set new_game.";
    }
    const std::vector<MapData> maps = overworld_maps(new_game);
    const MapPosition nurse{"VermilionCity_PokemonCenter_1F", map_point(maps, "VermilionCity_PokemonCenter_1F", "nurse_front")};
    if (nurse.point != TilePos{7, 4}){
        return "Unexpected nurse_front in Vermilion's Pokemon Center.";
    }
    if (!plans_from_cerulean_nurse(maps, RouteGoal{nurse, Direction::NORTH})){
        return "No route from Cerulean's Pokemon Center through the burgled house and the Underground Path to Vermilion's.";
    }
    const RouteGoal captain{MapPosition{"SSAnne_CaptainsOffice", {5, 5}}, Direction::NORTH};
    RouteCosts walking = default_route_costs();
    walking[RouteCost::SURF] = -1;
    const std::optional<std::vector<RouteStep>> aboard = plan_overworld_route(maps, nurse, captain, walking, Direction::NORTH);
    if (!aboard){
        return "No route from Vermilion's Pokemon Center onto the S.S. Anne to the Captain.";
    }
    //  The harbor's water (20..23, 23..27) is the shorter way south; a
    //  player without Surf walks round it.
    for (const RouteStep& step : *aboard){
        if (step.action == RouteAction::SURF){
            return "Expected the way aboard on land, not " + to_string(step) + ".";
        }
    }
    if (plan_overworld_route(all, nurse, captain, default_route_costs(), Direction::NORTH)){
        return "Expected the rival on the Captain's door to block the way while shown.";
    }
    return true;
}

//  A planned step walks where its map doesn't let the player run, and only
//  there: out of Pewter's Pokemon Center to the Gym.
UnitTestResult test_walk_only_indoors(){
    const std::vector<MapData> maps = overworld_maps();
    MapPosition from{"PewterCity_PokemonCenter_1F", map_point(maps, "PewterCity_PokemonCenter_1F", "nurse_front")};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps, from, RouteGoal{MapPosition{"PewterCity", map_point(maps, "PewterCity", "gym_front")}, Direction::NORTH}
    );
    if (!route){
        return "No route from Pewter's Pokemon Center to the Gym.";
    }
    bool indoors = false;
    bool outdoors = false;
    for (const RouteStep& step : *route){
        const bool in_center = from.map == "PewterCity_PokemonCenter_1F";
        if (step.action == RouteAction::WALK && step.walk_only != in_center){
            return "Wrong walk_only on " + to_string(step) + ", from " + to_string(from) + ".";
        }
        indoors = indoors || in_center;
        outdoors = outdoors || !in_center;
        from = step.end;
    }
    if (!indoors || !outdoors){
        return "Expected steps inside the Center and outside.";
    }
    const std::optional<WallHold> hold = hold_to_wall(maps, {"PewterCity_PokemonCenter_1F", {7, 4}}, false, Direction::WEST);
    if (!hold || !hold->walk_only){
        return "Expected a hold to a wall in the Center to walk.";
    }
    return true;
}

//  From (7, 6) in Route 4's Pokemon Center to the salesman, no leg holds
//  west along row 6: the escalator at (1, 6) rides up to 2F, which isn't
//  loaded (console B, 2026-11-02, "west 5 (to wall)").
UnitTestResult test_route4_center_escalator(){
    const std::vector<MapData> maps = overworld_maps();
    const MapPosition start{"Route4_PokemonCenter_1F", {7, 6}};
    const MapPosition salesman{"Route4_PokemonCenter_1F", {2, 3}};
    if (hold_to_wall(maps, start, false, Direction::WEST)){
        return "Expected no hold west from (7, 6) along the escalator's row.";
    }
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, RouteGoal{salesman, Direction::WEST});
    if (!route || !(follow_overworld_route(maps, start, *route) == salesman)){
        return "Expected a route from (7, 6) to the salesman.";
    }
    for (const RouteStep& step : *route){
        if (step.to_wall && step.direction == Direction::WEST && step.end.point == TilePos{2, 6}){
            return "Expected no hold west next to the escalator: " + to_string(step);
        }
    }
    return true;
}

//  A hall with an escalator at (1, 2) to a floor that isn't loaded, like
//  Route 4's Pokemon Center.
const char* const ESCALATOR_HALL = R"({
    "name": "Hall", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-02"},
    "tiles": ["######", "#....#", "#....#", "######"],
    "warps": [{"x": 1, "y": 2, "enter": "step_on"}]
})";

//  Holding west along the escalator's row rides it: a leg there ends by
//  counting tiles, never by holding to the wall behind it (console B,
//  2026-11-02, Route 4's Pokemon Center).
UnitTestResult test_no_hold_onto_escalator(){
    const std::vector<MapData> maps{parse_map(ESCALATOR_HALL, "Hall")};
    if (hold_to_wall(maps, {"Hall", {4, 2}}, false, Direction::WEST)){
        return "Expected no hold west along the escalator's row.";
    }
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, {"Hall", {4, 2}}, {{"Hall", {2, 1}}, std::nullopt});
    if (!route || !(follow_overworld_route(maps, {"Hall", {4, 2}}, *route) == MapPosition{"Hall", {2, 1}})){
        return "Expected a route from (4, 2) to (2, 1).";
    }
    for (const RouteStep& step : *route){
        if (step.to_wall && step.direction == Direction::WEST && step.end.point == TilePos{2, 2}){
            return "Expected no hold west next to the escalator: " + to_string(step);
        }
    }
    return true;
}


//  Whether the player, `walked` tiles into `step` from `at` when stopped,
//  is found again and walked back to the leg's end.
UnitTestResult check_found_again(
    const std::vector<MapData>& maps, const MapPosition& at, const RouteStep& step, WalkedTiles walked
){
    const std::vector<StopCandidate> candidates = stop_candidates(maps, at, step, walked);
    const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, step);
    if (!anchor || !(follow_overworld_route(maps, anchor->found, anchor->back) == step.end)){
        return std::format(
            "Not found again on {} from {}, stopped {} to {} tiles in ({} candidates).",
            to_string(step), to_string(at), walked.min, walked.max, candidates.size()
        );
    }
    return true;
}

//  Every open leg from `start` to `goal`, stopped wherever the timing may
//  put it, walking or running, or anywhere on it: the player is found again.
//  A leg to a warp off the grass is held on to its warp instead.
UnitTestResult check_found_again(const std::vector<MapData>& maps, const MapPosition& start, const MapPosition& goal){
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, RouteGoal{goal, std::nullopt});
    if (!route){
        return "No route from " + to_string(start) + ".";
    }
    MapPosition at = start;
    for (const RouteStep& step : *route){
        std::vector<WalkedTiles> stops;
        if (step.action == RouteAction::WALK && !step.to_wall && !(step.warps && !step.wild)){
            stops.push_back(whole_leg(step));
            for (int estimate = 0; estimate <= step.tiles; estimate++){
                stops.push_back(walked_near(step, estimate, std::chrono::milliseconds(267)));
                stops.push_back(walked_near(step, estimate, std::chrono::milliseconds(133)));
            }
        }
        for (const WalkedTiles& walked : stops){
            const UnitTestResult result = check_found_again(maps, at, step, walked);
            if (result.result != UnitTestResult::PASSED){
                return result;
            }
        }
        at = step.end;
    }
    return true;
}

//  00.11's way from Pewter into Route 3 and 00.12's over Route 3 and 4.
UnitTestResult test_found_again_route3_route4(){
    const std::vector<MapData> maps = overworld_maps();
    const UnitTestResult into_route3 = check_found_again(maps, {"PewterCity", {17, 26}}, {"Route3", {13, 8}});
    if (into_route3.result != UnitTestResult::PASSED){
        return into_route3;
    }
    return check_found_again(maps, {"Route3", {36, 8}}, {"Route4_PokemonCenter_1F", {7, 4}});
}

//  A leg off the grass that crosses one trainer's sight, on the maps the
//  story walks: a stop on it is that trainer's, on the tile they see, and
//  they stand next to the player afterwards.
UnitTestResult test_trainer_stops(){
    const std::vector<MapData> maps = overworld_maps();
    struct Case{
        const char* name;
        MapPosition start;
        Direction direction;
        int tiles;
        bool to_wall;
        MapPosition player;
        MapPosition trainer;
    };
    const Case cases[] = {
        {
            .name = "Route 3, Lass Robin, east along row 9 (00.12)",
            .start = {"Route3", {36, 9}}, .direction = Direction::EAST, .tiles = 11, .to_wall = true,
            .player = {"Route3", {40, 9}}, .trainer = {"Route3", {40, 10}},
        },
        {
            .name = "Viridian Forest, the Bug Catcher at (47, 45), north up column 42",
            .start = {"ViridianForest", {42, 61}}, .direction = Direction::NORTH, .tiles = 17, .to_wall = false,
            .player = {"ViridianForest", {42, 45}}, .trainer = {"ViridianForest", {43, 45}},
        },
    };
    for (const Case& c : cases){
        RouteStep step;
        step.direction = c.direction;
        step.tiles = c.tiles;
        step.to_wall = c.to_wall;
        step.end = c.start;
        step.end.point = TilePos{
            c.start.point.x + (c.direction == Direction::EAST ? c.tiles : 0),
            c.start.point.y - (c.direction == Direction::NORTH ? c.tiles : 0),
        };
        const std::vector<StopCandidate> candidates = stop_candidates(maps, c.start, step, whole_leg(step));
        if (candidates.size() != 1 || !(candidates[0].player == c.player) || !candidates[0].trainer || !(candidates[0].trainer->at == c.trainer)){
            return std::format("{}: expected one stop, at {} with the trainer at {}; got {} candidates.",
                c.name, to_string(c.player), to_string(c.trainer), candidates.size());
        }
        const std::optional<Reanchor> anchor = plan_reanchor(maps, candidates, step);
        if (!anchor || !anchor->holds.empty() || !(follow_overworld_route(maps, c.player, anchor->back) == step.end)){
            return std::string(c.name) + ": expected a way on from the stop, with no holds.";
        }
    }
    return true;
}

}


void add_tests_OverworldMaps(UnitTestDatabase& database){
    database.add<Test_OverworldMaps>("valid map", test_valid_map);
    database.add<Test_OverworldMaps>("missing field", test_missing_field);
    database.add<Test_OverworldMaps>("elevation left out", test_elevation_left_out);
    database.add<Test_OverworldMaps>("field of the wrong type", test_bad_field_type);
    database.add<Test_OverworldMaps>("row of the wrong length", test_bad_row_length);
    database.add<Test_OverworldMaps>("unknown tile", test_unknown_tile);
    database.add<Test_OverworldMaps>("not JSON", test_not_json);
    database.add<Test_OverworldMaps>("Route 3, Route 4 and its Pokemon Center", test_route3_route4_maps);
    database.add<Test_OverworldMaps>("Mt. Moon joins Route 4's cave mouths", test_mt_moon_maps);
    database.add<Test_OverworldMaps>("Mt. Moon's floor closed on one side is wild", test_mt_moon_closed_floor_wild);
    database.add<Test_OverworldMaps>("Cerulean's Mart and Gym, Route 25 and Bill's Sea Cottage", test_cerulean_maps);
    database.add<Test_OverworldMaps>("Route 5 and 6 to Vermilion City and the S.S. Anne", test_vermilion_maps);
    database.add<Test_OverworldMaps>("walks where running isn't allowed", test_walk_only_indoors);
    database.add<Test_OverworldMaps>("Route 4's Pokemon Center escalator", test_route4_center_escalator);
    database.add<Test_OverworldMaps>("trainer sight", test_trainer_sight);
    database.add<Test_OverworldMaps>("no hold onto an escalator", test_no_hold_onto_escalator);
    database.add<Test_OverworldMaps>("found again on Route 3 and 4", test_found_again_route3_route4);
    database.add<Test_OverworldMaps>("trainer stops on the story's maps", test_trainer_stops);
}



}
}
}
