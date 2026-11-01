/*  Overworld Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_LegPlanner.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::string to_string(const MapPosition& position){
    return position.map + " " + to_string(position.point);
}

std::string to_string(const RouteStep& step){
    std::string text;
    switch (step.action){
    case RouteAction::WALK:
        text = std::format("walk {} {}", direction_name(step.direction), step.tiles);
        if (step.warps){
            text += " (warp)";
        }
        if (step.to_wall){
            text += " (to wall)";
        }
        break;
    case RouteAction::SURF:
        text = "surf " + std::string(direction_name(step.direction));
        break;
    case RouteAction::FACE:
        text = "face " + std::string(direction_name(step.direction));
        break;
    }
    return text + " -> " + to_string(step.end);
}



const RouteCosts& default_route_costs(){
    static const RouteCosts COSTS{
        {RouteCost::TILE, 1},
        {RouteCost::TURN, 2},
        {RouteCost::WARP, 30},
        {RouteCost::SURF, 40},
        {RouteCost::GRASS_TILE, 20},
        {RouteCost::OPEN_STOP, 8},
    };
    return COSTS;
}

int route_cost(const RouteCosts& costs, RouteCost kind){
    auto iter = costs.find(kind);
    if (iter == costs.end()){
        iter = default_route_costs().find(kind);
    }
    return iter->second;
}



namespace{


int find_map(const std::vector<MapData>& maps, const std::string& name){
    for (size_t c = 0; c < maps.size(); c++){
        if (maps[c].name == name){
            return (int)c;
        }
    }
    return -1;
}

//  The player on one of the loaded maps.
struct Located{
    int map = 0;
    PlayerState state;

    bool operator==(const Located& other) const{
        return map == other.map
            && state.position == other.state.position
            && state.elevation == other.state.elevation
            && state.surfing == other.state.surfing;
    }
};
struct LocatedHash{
    size_t operator()(const Located& player) const{
        return (((std::hash<TilePos>()(player.state.position) * 31 + player.map) * 16
            + player.state.elevation) * 2) + (player.state.surfing ? 1 : 0);
    }
};

//  What pressing a direction once does to the player.
struct Transition{
    Located next;
    RouteAction action = RouteAction::WALK;
    bool warped = false;
    bool walked_out = false;     //  the warp ended with the game walking the player out
    bool dismounted = false;     //  stepped off the water onto land
};

//  The player taking `warp`, or nothing if it leads off the loaded maps.
std::optional<Transition> take_warp(const std::vector<MapData>& maps, const MapWarp* warp){
    if (warp == nullptr){
        return std::nullopt;
    }
    const int destination = find_map(maps, warp->dest_map);
    if (destination < 0 || warp->dest_warp < 0 || warp->dest_warp >= (int)maps[destination].warps.size()){
        return std::nullopt;
    }
    const MapData& map = maps[destination];
    const MapWarp& arrival = map.warps[warp->dest_warp];
    TilePos position{arrival.x, arrival.y};
    Transition transition;
    if (walks_out_on_arrival(map, position)){
        position = position.next(Direction::SOUTH);
        transition.walked_out = true;
    }
    transition.next.map = destination;
    transition.next.state = PlayerState{position, elevation_at(map, position), false};
    transition.warped = true;
    return transition;
}

//  Walking off the map's edge onto the map connected there, if it is loaded.
std::optional<Transition> cross_edge(const std::vector<MapData>& maps, const Located& player, Direction direction){
    const MapData& map = maps[player.map];
    for (const MapConnection& connection : map.connections){
        if (connection.direction != direction){
            continue;
        }
        const int destination = find_map(maps, connection.map);
        if (destination < 0){
            return std::nullopt;
        }
        const MapData& to = maps[destination];
        const TilePos here = player.state.position;
        TilePos point;
        switch (direction){
        case Direction::NORTH:
            point = {here.x - connection.offset, to.tiles.height - 1};
            break;
        case Direction::SOUTH:
            point = {here.x - connection.offset, 0};
            break;
        case Direction::WEST:
            point = {to.tiles.width - 1, here.y - connection.offset};
            break;
        case Direction::EAST:
            point = {0, here.y - connection.offset};
            break;
        }
        const StepKind kind = classify_step_onto(map, player.state, direction, to, point);
        if (kind == StepKind::BLOCKED){
            return std::nullopt;
        }
        Transition transition;
        transition.next.map = destination;
        transition.next.state = after_step_onto(map, player.state, to, point, kind);
        transition.action = kind == StepKind::START_SURF ? RouteAction::SURF : RouteAction::WALK;
        transition.dismounted = kind == StepKind::STOP_SURFING;
        return transition;
    }
    return std::nullopt;
}

//  Pressing `direction` once (field_control_avatar.c): an exit mat or a door
//  warps without the player moving, a step onto a ladder or cave entrance
//  warps after it, and anything else is a step by the collision rules.
std::optional<Transition> press(const std::vector<MapData>& maps, const Located& player, Direction direction){
    const MapData& map = maps[player.map];
    const TilePos here = player.state.position;
    if (exit_mat(map, here) == direction){
        return take_warp(maps, warp_at(map, here));
    }
    const TilePos ahead = here.next(direction);
    if (direction == Direction::NORTH && is_door(map, ahead) && !player.state.surfing){
        return take_warp(maps, warp_at(map, ahead));
    }

    if (!in_bounds(map, ahead)){
        return cross_edge(maps, player, direction);
    }

    const StepKind kind = classify_step(map, player.state, direction);
    if (kind == StepKind::BLOCKED){
        return std::nullopt;
    }
    Transition transition;
    transition.next = player;
    transition.next.state = after_step(map, player.state, direction, kind);
    transition.action = kind == StepKind::START_SURF ? RouteAction::SURF : RouteAction::WALK;
    transition.dismounted = kind == StepKind::STOP_SURFING;
    const TilePos landed = transition.next.state.position;
    if (transition.action == RouteAction::WALK && warps_on_step(map, landed) && warp_at(map, landed) != nullptr){
        return take_warp(maps, warp_at(map, landed));
    }
    return transition;
}

//  Whether holding `direction` stops the player where they are: a wall, a
//  ledge, or water they'd need to Surf onto. A leg that ends there can be
//  held for longer than it takes, and walked again after an interruption. A
//  person in the way doesn't count, since they may have wandered off, and
//  neither does an edge to a map that isn't loaded.
bool stops_at(const std::vector<MapData>& maps, const Located& player, Direction direction){
    const std::optional<Transition> transition = press(maps, player, direction);
    if (transition){
        return transition->action == RouteAction::SURF;
    }
    const MapData& map = maps[player.map];
    const TilePos ahead = player.state.position.next(direction);
    if (in_bounds(map, ahead)){
        return object_at(map, ahead) == nullptr;
    }
    for (const MapConnection& connection : map.connections){
        if (connection.direction == direction){
            return false;
        }
    }
    return true;
}

//  The search state: a player, the way they face (4 stands for "not known
//  yet", before the first step), and whether they are in the middle of a leg
//  that could go on, as opposed to just stopped by a warp or Surf.
//  The game's step rules for the shared leg planner.
class StepModel{
public:
    using State = Located;
    using StateHash = LocatedHash;

    StepModel(const std::vector<MapData>& maps, const RouteCosts& costs)
        : m_maps(maps)
        , m_costs(costs)
    {}

    std::optional<Pokemon::Step<State>> press(const State& state, Direction direction) const{
        const std::optional<Transition> transition = PokemonFRLG::press(m_maps, state, direction);
        if (!transition){
            return std::nullopt;
        }
        Pokemon::Step<State> step;
        step.next = transition->next;
        step.action = transition->action == RouteAction::SURF ? ACTION_SURF : 0;
        //  Leaving the water: the player walks at half the speed from here.
        step.starts_leg = transition->dismounted;
        step.ends_leg = transition->warped || transition->dismounted;
        step.warped = transition->warped;
        step.facing = transition->walked_out ? Direction::SOUTH : direction;
        if (transition->action == RouteAction::SURF){
            step.extra_cost += route_cost(m_costs, RouteCost::SURF);
        }
        if (transition->warped){
            step.extra_cost += route_cost(m_costs, RouteCost::WARP);
        }
        if (is_tall_grass(m_maps[step.next.map], step.next.state.position)){
            step.extra_cost += route_cost(m_costs, RouteCost::GRASS_TILE);
        }
        return step;
    }
    bool held_at(const State& state, Direction direction) const{
        return stops_at(m_maps, state, direction);
    }

    static constexpr int ACTION_SURF = 1;

private:
    const std::vector<MapData>& m_maps;
    const RouteCosts& m_costs;
};

Pokemon::LegCosts to_leg_costs(const RouteCosts& costs){
    Pokemon::LegCosts leg_costs;
    leg_costs.per_tile = route_cost(costs, RouteCost::TILE);
    leg_costs.per_turn = route_cost(costs, RouteCost::TURN);
    leg_costs.per_open_stop = route_cost(costs, RouteCost::OPEN_STOP);
    return leg_costs;
}

RouteStep to_route_step(const std::vector<MapData>& maps, const Pokemon::Leg<Located>& leg){
    RouteStep step;
    switch (leg.kind){
    case Pokemon::LegKind::WALK:
        step.action = RouteAction::WALK;
        break;
    case Pokemon::LegKind::ACTION:
        step.action = RouteAction::SURF;
        break;
    case Pokemon::LegKind::FACE:
        step.action = RouteAction::FACE;
        break;
    }
    step.direction = leg.direction;
    step.tiles = leg.kind == Pokemon::LegKind::FACE ? 0 : leg.tiles;
    step.warps = leg.warps;
    step.surfing = leg.start.state.surfing;
    step.to_wall = leg.to_wall;
    step.facing_after = leg.facing_after;
    step.end = MapPosition{maps[leg.end.map].name, leg.end.state.position};
    return step;
}

}



std::optional<std::vector<RouteStep>> plan_overworld_route(
    const std::vector<MapData>& maps,
    const MapPosition& start,
    const RouteGoal& goal,
    const RouteCosts& costs,
    std::optional<Direction> start_facing
){
    const int start_map = find_map(maps, start.map);
    const int goal_map = find_map(maps, goal.position.map);
    if (start_map < 0 || goal_map < 0){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route between maps that aren't loaded.");
    }
    Located first;
    first.map = start_map;
    first.state.position = start.point;
    first.state.elevation = elevation_at(maps[start_map], start.point);
    first.state.surfing = is_water(maps[start_map], start.point);

    const StepModel model(maps, costs);
    const auto legs = Pokemon::plan_legs(
        model, first, start_facing,
        std::function<bool(const Located&)>([&](const Located& player){
            return player.map == goal_map && player.state.position == goal.position.point;
        }),
        goal.facing, to_leg_costs(costs)
    );
    if (!legs){
        return std::nullopt;
    }

    std::vector<RouteStep> route;
    for (const Pokemon::Leg<Located>& leg : *legs){
        route.push_back(to_route_step(maps, leg));
    }
    return route;
}


MapPosition follow_overworld_route(
    const std::vector<MapData>& maps,
    const MapPosition& start,
    const std::vector<RouteStep>& route
){
    Located player;
    player.map = find_map(maps, start.map);
    if (player.map < 0){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route from a map that isn't loaded: " + start.map);
    }
    player.state.position = start.point;
    player.state.elevation = elevation_at(maps[player.map], start.point);
    player.state.surfing = is_water(maps[player.map], start.point);

    for (const RouteStep& step : route){
        if (step.action == RouteAction::FACE){
            continue;
        }
        for (int c = 0; c < step.tiles; c++){
            const std::optional<Transition> transition = press(maps, player, step.direction);
            if (!transition || transition->action != step.action){
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    "Step refused at " + to_string(MapPosition{maps[player.map].name, player.state.position}) + ": " + to_string(step)
                );
            }
            player = transition->next;
        }
        const MapPosition position{maps[player.map].name, player.state.position};
        if (!(position == step.end)){
            throw InternalProgramError(
                nullptr, PA_CURRENT_FUNCTION,
                "Ended at " + to_string(position) + " instead of " + to_string(step.end)
            );
        }
    }
    return MapPosition{maps[player.map].name, player.state.position};
}



namespace{

class Test_OverworldRoute : public UnitTest{
public:
    Test_OverworldRoute(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::OverworldRoute - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (FileException& e){
            //  CI runs with upstream Packages, which may not have the maps yet.
            return UnitTestResult(UnitTestResult::SKIPPED, "Map files not found: " + e.message());
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
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
        "name": "Small", "width": 5, "height": 4,
        "tiles": ["#####", "#.n.#", "#.~.#", "#####"],
        "elevation": ["33333", "33403", "33133", "33333"],
        "warps": [], "objects": [], "connections": [], "points": [], "flag_sets": []
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
    if (mewtwo == nullptr || mewtwo->kind != "mewtwo"){
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

//  Two small maps that don't need the map files: a room left by the exit mat
//  at its bottom, onto the door of a yard, out of which the game walks the
//  player.
const char* const TEST_ROOM_JSON = R"({
    "name": "Room", "width": 5, "height": 4,
    "tiles": ["#####", "#...#", "#...#", "##v##"],
    "elevation": ["33333", "33333", "33333", "33333"],
    "warps": [{"x": 2, "y": 3, "dest_map": "Yard", "dest_warp": 0}],
    "objects": [],
    "connections": [], "points": [], "flag_sets": []
})";
const char* const TEST_YARD_JSON = R"({
    "name": "Yard", "width": 5, "height": 4,
    "tiles": ["#####", "##D##", "#...#", "#####"],
    "elevation": ["33333", "33333", "33333", "33333"],
    "warps": [{"x": 2, "y": 1, "dest_map": "Room", "dest_warp": 0}],
    "objects": [],
    "connections": [], "points": [], "flag_sets": []
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

}

void add_tests_OverworldRoute(UnitTestDatabase& database){
    database.add<Test_OverworldRoute>("route through a warp", test_route_through_warp);
    database.add<Test_OverworldRoute>("maps to Brock", test_story_maps);
    database.add<Test_OverworldRoute>("bedroom to Oak's Lab", test_bedroom_to_oaks_lab);
    database.add<Test_OverworldRoute>("Viridian Mart to Pewter Gym", test_viridian_mart_to_pewter_gym);
    database.add<Test_OverworldRoute>("step rules", test_step_rules);
    database.add<Test_OverworldRoute>("maps", test_maps);
    database.add<Test_OverworldRoute>("Cerulean Cave entrance to Mewtwo", test_entrance_to_mewtwo);
    database.add<Test_OverworldRoute>("Cerulean Pokemon Center to Mewtwo", test_pokemon_center_to_mewtwo);
    database.add<Test_OverworldRoute>("cave guard blocks the way", test_cave_guard_blocks);
    database.add<Test_OverworldRoute>("Mewtwo's tile is blocked", test_mewtwo_tile_unreachable);
}



}
}
}
