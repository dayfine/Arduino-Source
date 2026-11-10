/*  Overworld Route Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Moved from PokemonFRLG_OverworldRoute.cpp, the tests registered from a
 *  table of cases.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct RouteTestInput{
    std::string name;
    UnitTestResult (*body)();
};
struct RouteTestExpectation{
    const char* description;
};
struct RouteTestCase{
    RouteTestInput input;
    RouteTestExpectation expectation;
};

class Test_OverworldRoute : public UnitTest{
public:
    explicit Test_OverworldRoute(RouteTestCase test_case)
        : UnitTest("PokemonFRLG::OverworldRoute - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    RouteTestCase m_case;
};

std::string describe(const std::vector<RouteStep>& route){
    std::string text;
    for (const RouteStep& step : route){
        text += "\n  " + to_string(step);
    }
    return text;
}

//  A 5x4 map for the step rules: a pond, a raised ledge reached by a stair
//  tile, and a tile that can't be entered moving south.
const MapData& small_map(){
    static const MapData MAP = parse_map(R"({
        "name": "Small", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
        "tiles": ["#####", "#.8.#", "#.w.#", "#####"],
        "elevation": ["33333", "33403", "33133", "33333"]
    })", "Small");
    return MAP;
}

UnitTestResult test_step_rules(){
    const MapData& map = small_map();
    PlayerState state{{1, 1}, 3, false};
    if (classify_step(map, state, Direction::EAST) != StepKind::BLOCKED){
        return "Walked from elevation 3 onto elevation 4.";
    }
    if (classify_step(map, PlayerState{{2, 0}, 3, false}, Direction::SOUTH) != StepKind::BLOCKED){
        return "Entered a north-blocked tile moving south.";
    }
    state = PlayerState{{1, 2}, 3, false};
    if (classify_step(map, state, Direction::EAST) != StepKind::START_SURF){
        return "Can't start surfing onto the pond.";
    }
    state = after_step(map, state, Direction::EAST, StepKind::START_SURF);
    if (!state.surfing || state.elevation != 1){
        return "Surfing didn't put the player on the water.";
    }
    if (classify_step(map, state, Direction::EAST) != StepKind::STOP_SURFING){
        return "Can't land from the pond.";
    }
    if (classify_step(map, PlayerState{{3, 2}, 3, false}, Direction::NORTH) != StepKind::WALK){
        return "Can't step from elevation 3 onto the elevation-0 stair.";
    }
    return true;
}

//  The maps after the Hall of Fame, which clears the way to Cerulean Cave.
std::vector<MapData> post_game_maps(){
    return overworld_maps(map_flag_set(overworld_maps(), "after_hall_of_fame"));
}

MapPosition point_on(const std::vector<MapData>& maps, const std::string& map, const std::string& point){
    return MapPosition{map, map_point(maps, map, point)};
}

const MapData* map_named(const std::vector<MapData>& maps, const std::string& name){
    const int index = find_map(maps, name);
    return index < 0 ? nullptr : &maps[index];
}

UnitTestResult test_maps(){
    const std::vector<MapData> maps = post_game_maps();
    const MapData* b1f = map_named(maps, "CeruleanCave_B1F");
    const MapData* city = map_named(maps, "CeruleanCity");
    if (b1f == nullptr || city == nullptr || map_named(maps, "CeruleanCity_PokemonCenter_1F") == nullptr){
        return "Missing a map.";
    }
    if (b1f->tiles.width != 40 || b1f->tiles.height != 23 || city->tiles.width != 48 || city->tiles.height != 40){
        return "Unexpected map size.";
    }
    const TilePos mewtwo_tile = map_point(maps, "CeruleanCave_B1F", "mewtwo");
    const MapObject* mewtwo = object_at(*b1f, mewtwo_tile);
    if (mewtwo == nullptr || mewtwo->name != "mewtwo"){
        return "Mewtwo is not on B1F at " + to_string(mewtwo_tile) + ".";
    }
    if (object_at(*city, {1, 13}) != nullptr){
        return "The cave guard is still there after the Hall of Fame.";
    }
    if (object_at(*map_named(overworld_maps(), "CeruleanCity"), {1, 13}) == nullptr){
        return "The cave guard is missing before the Hall of Fame.";
    }
    return true;
}

//  Plans `start` to `goal`, replays it with the step rules and checks where it ends.
UnitTestResult check_route(
    const std::vector<MapData>& maps,
    const MapPosition& start, const RouteGoal& goal,
    std::vector<RouteStep>& route
){
    const std::optional<std::vector<RouteStep>> planned = plan_overworld_route(maps, start, goal);
    if (!planned){
        return "No route from " + to_string(start) + " to " + to_string(goal.position) + ".";
    }
    route = *planned;
    const MapPosition end = follow_overworld_route(maps, start, route);
    if (!(end == goal.position)){
        return "Route ends at " + to_string(end) + ":" + describe(route);
    }
    if (goal.facing && route.back().direction != *goal.facing){
        return "Doesn't end facing " + std::string(direction_name(*goal.facing)) + ":" + describe(route);
    }
    return true;
}

//  Below Mewtwo, facing it, to talk to it.
RouteGoal facing_mewtwo(const std::vector<MapData>& maps){
    const TilePos mewtwo = map_point(maps, "CeruleanCave_B1F", "mewtwo");
    return RouteGoal{{"CeruleanCave_B1F", mewtwo.next(Direction::SOUTH)}, Direction::NORTH};
}

UnitTestResult test_entrance_to_mewtwo(){
    const std::vector<MapData> maps = post_game_maps();
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(maps, point_on(maps, "CeruleanCave_1F", "entrance"), facing_mewtwo(maps), route);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    bool surfs = false;
    bool reaches_b1f = false;
    for (const RouteStep& step : route){
        surfs |= step.action == RouteAction::SURF;
        reaches_b1f |= step.warps && step.end.map == "CeruleanCave_B1F";
    }
    if (!surfs || !reaches_b1f){
        return "Expected to take the ladder to B1F and to surf:" + describe(route);
    }
    return true;
}

UnitTestResult test_pokemon_center_to_mewtwo(){
    const std::vector<MapData> maps = post_game_maps();
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(
        maps, point_on(maps, "CeruleanCity_PokemonCenter_1F", "pc"), facing_mewtwo(maps), route
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    //  Out through the exit mat, onto the city's step below the door, and
    //  into the cave by its entrance.
    std::vector<std::string> maps_in_order;
    for (const RouteStep& step : route){
        if (step.warps){
            maps_in_order.push_back(step.end.map);
        }
    }
    if (maps_in_order.size() < 2 || maps_in_order[0] != "CeruleanCity" || maps_in_order[1] != "CeruleanCave_1F"){
        return "Expected to leave by the mat and enter the cave next:" + describe(route);
    }
    if (!(route[0].end.map == "CeruleanCity_PokemonCenter_1F" || route[0].warps)){
        return "Unexpected first step:" + describe(route);
    }
    return true;
}

UnitTestResult test_cave_guard_blocks(){
    //  Before the Hall of Fame the guard stands in the cave entrance.
    const std::vector<MapData> maps = overworld_maps();
    if (plan_overworld_route(maps, point_on(maps, "CeruleanCity_PokemonCenter_1F", "pc"), facing_mewtwo(maps))){
        return "Planned past the cave guard.";
    }
    return true;
}

UnitTestResult test_mewtwo_tile_unreachable(){
    const std::vector<MapData> maps = post_game_maps();
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps,
        point_on(maps, "CeruleanCave_1F", "entrance"),
        {point_on(maps, "CeruleanCave_B1F", "mewtwo"), std::nullopt}
    );
    if (route){
        return "Planned a route onto Mewtwo itself:" + describe(*route);
    }
    return true;
}

UnitTestResult test_story_maps(){
    const std::vector<MapData> maps = overworld_maps();
    for (const char* name : {
        "PalletTown_PlayersHouse_2F", "PalletTown_ProfessorOaksLab", "Route1", "ViridianCity_Mart",
        "Route22", "Route2", "ViridianForest", "PewterCity", "PewterCity_Museum_1F", "PewterCity_Gym",
    }){
        if (map_named(maps, name) == nullptr){
            return std::string("Missing map ") + name + ".";
        }
    }
    return true;
}

//  Which maps `route` warps into, in order.
std::vector<std::string> maps_warped_into(const std::vector<RouteStep>& route){
    std::vector<std::string> maps;
    for (const RouteStep& step : route){
        if (step.warps){
            maps.push_back(step.end.map);
        }
    }
    return maps;
}

UnitTestResult test_bedroom_to_oaks_lab(){
    const std::vector<MapData> maps = overworld_maps();
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(
        maps, point_on(maps, "PalletTown_PlayersHouse_2F", "start"),
        {point_on(maps, "PalletTown", "oaks_lab_front"), Direction::NORTH}, route
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    const std::vector<std::string> warped = maps_warped_into(route);
    if (warped != std::vector<std::string>{"PalletTown_PlayersHouse_1F", "PalletTown"}){
        return "Expected to take the stairs and leave by the front door:" + describe(route);
    }
    return true;
}

UnitTestResult test_viridian_mart_to_pewter_gym(){
    const std::vector<MapData> maps = overworld_maps();
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(
        maps, point_on(maps, "ViridianCity_Mart", "clerk_front"),
        {point_on(maps, "PewterCity", "gym_front"), Direction::NORTH}, route
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    const std::vector<std::string> warped = maps_warped_into(route);
    if (std::find(warped.begin(), warped.end(), "ViridianForest") == warped.end()){
        return "Expected to go through Viridian Forest:" + describe(route);
    }
    return true;
}

//  A room with a sign on its north wall, above the third column.
const char* const TEST_SIGN_ROOM_JSON = R"({
    "name": "SignRoom", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["##S##", "#...#", "#...#", "#...#", "#####"],
    "elevation": ["33333", "33333", "33333", "33333", "33333"]
})";

//  Walking north under a sign stops by counting tiles, never by holding into
//  the sign; under the wall beside it, the wall stops the player.
UnitTestResult test_no_hold_into_sign(){
    const std::vector<MapData> maps{parse_map(TEST_SIGN_ROOM_JSON, "SignRoom")};
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(maps, {"SignRoom", {2, 3}}, {{"SignRoom", {2, 1}}, std::nullopt}, route);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    for (const RouteStep& step : route){
        if (step.to_wall && step.end.point == TilePos{2, 1}){
            return "Expected no hold into the sign:" + describe(route);
        }
    }
    route.clear();
    result = check_route(maps, {"SignRoom", {1, 3}}, {{"SignRoom", {1, 1}}, std::nullopt}, route);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    if (route.empty() || !route.back().to_wall){
        return "Expected the wall to stop the player beside the sign:" + describe(route);
    }
    return true;
}

//  A yard with a row of ledges across it.
const char* const TEST_LEDGE_YARD_JSON = R"({
    "name": "LedgeYard", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["#####", "#...#", "#...#", "#vvv#", "#...#", "#####"],
    "elevation": ["33333", "33333", "33333", "33333", "33333", "33333"]
})";

//  A ledge isn't a wall to hold into: moving the way it faces the player
//  would hop it, and routes don't hop ledges. Viridian City,
//  from the Pokemon Center's door south to the row above the ledge.
UnitTestResult test_no_hold_into_ledge(){
    const std::vector<MapData> maps{parse_map(TEST_LEDGE_YARD_JSON, "LedgeYard")};
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(maps, {"LedgeYard", {2, 1}}, {{"LedgeYard", {2, 2}}, std::nullopt}, route);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    for (const RouteStep& step : route){
        if (step.to_wall){
            return "Expected no hold into the ledge:" + describe(route);
        }
    }
    route.clear();
    const std::vector<MapData> world = overworld_maps();
    result = check_route(world, {"ViridianCity", {26, 27}}, {{"ViridianCity", {26, 29}}, std::nullopt}, route);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    for (const RouteStep& step : route){
        if (step.to_wall && step.end.point == TilePos{26, 29}){
            return "Expected no hold into the ledge below the Pokemon Center:" + describe(route);
        }
    }
    return true;
}

//  Two small maps that don't need the map files: a room left by the exit mat
//  at its bottom, onto the door of a yard, out of which the game walks the
//  player.
//  No wild battle can start between the bedroom and Oak's Lab: Pallet Town's
//  streets have no tall grass. Route 2 and Viridian Forest do.
UnitTestResult test_wild_steps(){
    const std::vector<MapData> maps = overworld_maps();
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(
        maps, point_on(maps, "PalletTown_PlayersHouse_2F", "start"),
        {point_on(maps, "PalletTown", "oaks_lab_front"), Direction::NORTH}, route
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    for (const RouteStep& step : route){
        if (step.wild){
            return "Expected no wild step from the bedroom to Oak's Lab, got " + to_string(step) + ":" + describe(route);
        }
    }
    route.clear();
    result = check_route(
        maps, point_on(maps, "ViridianCity_Mart", "clerk_front"),
        {point_on(maps, "PewterCity", "gym_front"), Direction::NORTH}, route
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    bool forest = false;
    for (const RouteStep& step : route){
        forest = forest || (step.wild && step.end.map == "ViridianForest");
    }
    if (!forest){
        return "Expected a wild step in Viridian Forest:" + describe(route);
    }
    //  The leg from Pallet Town north onto Route 1 crosses Route 1's grass
    //  after the map's edge.
    route.clear();
    result = check_route(
        maps, point_on(maps, "PalletTown", "oaks_lab_front"),
        {point_on(maps, "ViridianCity_Mart", "clerk_front"), std::nullopt}, route
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    for (const RouteStep& step : route){
        if (step.end.map == "Route1" && step.direction == Direction::NORTH && step.tiles > 14 && !step.wild){
            return "Expected the leg from Pallet Town onto Route 1 to be wild, got " + to_string(step) + ":" + describe(route);
        }
    }
    return true;
}

const char* const TEST_ROOM_JSON = R"({
    "name": "Room", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["#####", "#...#", "#...#", "##.##"],
    "elevation": ["33333", "33333", "33333", "33333"],
    "points": {"warp_0": [2, 3]},
    "warps": [{"x": 2, "y": 3, "enter": "walk_south", "to_map": "Yard", "to_point": "warp_0"}]
})";
const char* const TEST_YARD_JSON = R"({
    "name": "Yard", "source": {"decomp_file": "", "revision": "test", "date": "2026-10-31"},
    "tiles": ["#####", "##D##", "#...#", "#####"],
    "elevation": ["33333", "33333", "33333", "33333"],
    "points": {"warp_0": [2, 1]},
    "warps": [{"x": 2, "y": 1, "enter": "walk_into", "to_map": "Room", "to_point": "warp_0"}]
})";

UnitTestResult test_route_through_warp(){
    const std::vector<MapData> maps{
        parse_map(TEST_ROOM_JSON, "Room"),
        parse_map(TEST_YARD_JSON, "Yard"),
    };
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(maps, {"Room", {2, 1}}, {{"Yard", {1, 2}}, std::nullopt}, route);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    for (const RouteStep& step : route){
        if (step.warps && step.end.map == "Yard"){
            return true;
        }
    }
    return "Expected to warp into the yard:" + describe(route);
}

//  `json` with its elevation grid replaced by `rows`, or left out if empty.
std::string with_elevation(std::string json, const std::string& rows){
    const size_t begin = json.find("\"elevation\"");
    const size_t end = json.find("\n", begin);
    json.erase(begin, end - begin + 1);
    if (!rows.empty()){
        json.insert(begin, "\"elevation\": " + rows + ",\n");
    }
    return json;
}

//  Room and yard as the decompilation writes them, their walls and the
//  door at elevation 0, plan the same as with no elevation grid, which
//  frlg_maps.py leaves out when every tile the player can stand on is ground.
UnitTestResult test_ground_only_elevation_left_out(){
    const std::vector<MapData> written{
        parse_map(with_elevation(TEST_ROOM_JSON, R"(["00000", "03330", "03330", "00300"])"), "Room"),
        parse_map(with_elevation(TEST_YARD_JSON, R"(["00000", "00000", "03330", "00000"])"), "Yard"),
    };
    const std::vector<MapData> left_out{
        parse_map(with_elevation(TEST_ROOM_JSON, ""), "Room"),
        parse_map(with_elevation(TEST_YARD_JSON, ""), "Yard"),
    };
    for (const MapPosition& start : {MapPosition{"Room", {1, 1}}, MapPosition{"Room", {3, 2}}}){
        for (const MapPosition& goal : {MapPosition{"Yard", {1, 2}}, MapPosition{"Yard", {3, 2}}, MapPosition{"Room", {2, 2}}}){
            const std::optional<std::vector<RouteStep>> a = plan_overworld_route(written, start, {goal, std::nullopt});
            const std::optional<std::vector<RouteStep>> b = plan_overworld_route(left_out, start, {goal, std::nullopt});
            if (!a || !b || describe(*a) != describe(*b)){
                return "Plans differ from " + to_string(start) + " to " + to_string(goal) + ":"
                    + (a ? describe(*a) : " none") + "\nwithout elevation:" + (b ? describe(*b) : " none");
            }
        }
    }
    return true;
}

const RouteTestCase ROUTE_CASES[] = {
    {
        .input = {.name = "route through a warp", .body = test_route_through_warp},
        .expectation = {.description = "route through a warp"},
    },
    {
        .input = {.name = "ground-only elevation left out", .body = test_ground_only_elevation_left_out},
        .expectation = {.description = "ground-only elevation left out"},
    },
    {
        .input = {.name = "maps to Brock", .body = test_story_maps},
        .expectation = {.description = "maps to Brock"},
    },
    {
        .input = {.name = "bedroom to Oak's Lab", .body = test_bedroom_to_oaks_lab},
        .expectation = {.description = "bedroom to Oak's Lab"},
    },
    {
        .input = {.name = "Viridian Mart to Pewter Gym", .body = test_viridian_mart_to_pewter_gym},
        .expectation = {.description = "Viridian Mart to Pewter Gym"},
    },
    {
        .input = {.name = "wild steps", .body = test_wild_steps},
        .expectation = {.description = "wild steps"},
    },
    {
        .input = {.name = "no hold into a sign", .body = test_no_hold_into_sign},
        .expectation = {.description = "no hold into a sign"},
    },
    {
        .input = {.name = "no hold into a ledge", .body = test_no_hold_into_ledge},
        .expectation = {.description = "no hold into a ledge"},
    },
    {
        .input = {.name = "step rules", .body = test_step_rules},
        .expectation = {.description = "step rules"},
    },
    {
        .input = {.name = "maps", .body = test_maps},
        .expectation = {.description = "maps"},
    },
    {
        .input = {.name = "Cerulean Cave entrance to Mewtwo", .body = test_entrance_to_mewtwo},
        .expectation = {.description = "Cerulean Cave entrance to Mewtwo"},
    },
    {
        .input = {.name = "Cerulean Pokemon Center to Mewtwo", .body = test_pokemon_center_to_mewtwo},
        .expectation = {.description = "Cerulean Pokemon Center to Mewtwo"},
    },
    {
        .input = {.name = "cave guard blocks the way", .body = test_cave_guard_blocks},
        .expectation = {.description = "cave guard blocks the way"},
    },
    {
        .input = {.name = "Mewtwo's tile is blocked", .body = test_mewtwo_tile_unreachable},
        .expectation = {.description = "Mewtwo's tile is blocked"},
    },
};

} // namespace

void add_tests_OverworldRoute(UnitTestDatabase& database){
    for (const RouteTestCase& item : ROUTE_CASES){
        database.add<Test_OverworldRoute>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
