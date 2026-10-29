/*  Overworld Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
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
        text = "walk " + std::string(direction_name(step.direction)) + " " + std::to_string(step.tiles);
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



namespace{

constexpr MapDirection DIRECTIONS[] = {
    MapDirection::NORTH, MapDirection::EAST, MapDirection::SOUTH, MapDirection::WEST,
};

int find_map(const std::vector<OverworldMap>& maps, const std::string& name){
    for (size_t c = 0; c < maps.size(); c++){
        if (maps[c].name() == name){
            return (int)c;
        }
    }
    return -1;
}

//  The player on one of the loaded maps.
struct Located{
    int map = 0;
    PlayerState state;
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
std::optional<Transition> take_warp(const std::vector<OverworldMap>& maps, const MapWarp* warp){
    if (warp == nullptr){
        return std::nullopt;
    }
    const int destination = find_map(maps, warp->dest_map);
    if (destination < 0 || warp->dest_warp < 0 || warp->dest_warp >= (int)maps[destination].warps().size()){
        return std::nullopt;
    }
    const OverworldMap& map = maps[destination];
    const MapWarp& arrival = map.warps()[warp->dest_warp];
    MapPoint position{arrival.x, arrival.y};
    Transition transition;
    if (map.walks_out_on_arrival(position)){
        position = position.next(MapDirection::SOUTH);
        transition.walked_out = true;
    }
    transition.next.map = destination;
    transition.next.state = PlayerState{position, map.elevation(position), false};
    transition.warped = true;
    return transition;
}

//  Walking off the map's edge onto the map connected there, if it is loaded.
std::optional<Transition> cross_edge(const std::vector<OverworldMap>& maps, const Located& player, MapDirection direction){
    const OverworldMap& map = maps[player.map];
    for (const MapConnection& connection : map.connections()){
        if (connection.direction != direction){
            continue;
        }
        const int destination = find_map(maps, connection.map);
        if (destination < 0){
            return std::nullopt;
        }
        const OverworldMap& to = maps[destination];
        const MapPoint here = player.state.position;
        MapPoint point;
        switch (direction){
        case MapDirection::NORTH:
            point = {here.x - connection.offset, to.height() - 1};
            break;
        case MapDirection::SOUTH:
            point = {here.x - connection.offset, 0};
            break;
        case MapDirection::WEST:
            point = {to.width() - 1, here.y - connection.offset};
            break;
        case MapDirection::EAST:
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
std::optional<Transition> press(const std::vector<OverworldMap>& maps, const Located& player, MapDirection direction){
    const OverworldMap& map = maps[player.map];
    const MapPoint here = player.state.position;
    if (map.exit_mat(here) == direction){
        return take_warp(maps, map.warp_at(here));
    }
    const MapPoint ahead = here.next(direction);
    if (direction == MapDirection::NORTH && map.door(ahead) && !player.state.surfing){
        return take_warp(maps, map.warp_at(ahead));
    }

    if (!map.in_bounds(ahead)){
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
    const MapPoint landed = transition.next.state.position;
    if (transition.action == RouteAction::WALK && map.warps_on_step(landed) && map.warp_at(landed) != nullptr){
        return take_warp(maps, map.warp_at(landed));
    }
    return transition;
}

//  Whether holding `direction` stops the player where they are: a wall, a
//  ledge, or water they'd need to Surf onto. A leg that ends there can be
//  held for longer than it takes, and walked again after an interruption. A
//  person in the way doesn't count, since they may have wandered off, and
//  neither does an edge to a map that isn't loaded.
bool stops_at(const std::vector<OverworldMap>& maps, const Located& player, MapDirection direction){
    const std::optional<Transition> transition = press(maps, player, direction);
    if (transition){
        return transition->action == RouteAction::SURF;
    }
    const OverworldMap& map = maps[player.map];
    const MapPoint ahead = player.state.position.next(direction);
    if (map.in_bounds(ahead)){
        return map.object_at(ahead) == nullptr;
    }
    for (const MapConnection& connection : map.connections()){
        if (connection.direction == direction){
            return false;
        }
    }
    return true;
}

//  The search state: a player, the way they face (4 stands for "not known
//  yet", before the first step), and whether they are in the middle of a leg
//  that could go on, as opposed to just stopped by a warp or Surf.
struct SearchState{
    Located player;
    int facing = 4;
    bool moving = false;
};

class StateIndex{
public:
    explicit StateIndex(const std::vector<OverworldMap>& maps){
        for (const OverworldMap& map : maps){
            m_width = std::max(m_width, map.width());
            m_height = std::max(m_height, map.height());
        }
    }
    size_t operator()(const SearchState& s) const{
        size_t index = s.player.map;
        index = index * m_height + s.player.state.position.y;
        index = index * m_width + s.player.state.position.x;
        index = index * 16 + s.player.state.elevation;
        index = index * 2 + (s.player.state.surfing ? 1 : 0);
        index = index * 5 + s.facing;
        return index * 2 + (s.moving ? 1 : 0);
    }

private:
    int m_width = 0;
    int m_height = 0;
};

//  One single-tile move of the search.
struct Move{
    size_t from = 0;
    MapDirection direction = MapDirection::NORTH;
    RouteAction action = RouteAction::WALK;
    bool warped = false;
    bool dismounted = false;
    bool walked_out = false;
};

}



std::optional<std::vector<RouteStep>> plan_overworld_route(
    const std::vector<OverworldMap>& maps,
    const MapPosition& start,
    const RouteGoal& goal,
    const RouteCosts& costs
){
    const int start_map = find_map(maps, start.map);
    const int goal_map = find_map(maps, goal.position.map);
    if (start_map < 0 || goal_map < 0){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route between maps that aren't loaded.");
    }

    StateIndex index_of(maps);
    const int INF = std::numeric_limits<int>::max();
    std::unordered_map<size_t, int> distance;
    std::unordered_map<size_t, Move> came_from;
    std::unordered_map<size_t, SearchState> states;
    auto distance_of = [&](size_t index){
        auto iter = distance.find(index);
        return iter == distance.end() ? INF : iter->second;
    };

    SearchState first;
    first.player.map = start_map;
    first.player.state.position = start.point;
    first.player.state.elevation = maps[start_map].elevation(start.point);
    first.player.state.surfing = maps[start_map].water(start.point);
    const size_t first_index = index_of(first);
    distance[first_index] = 0;
    states[first_index] = first;

    using Entry = std::pair<int, size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    queue.emplace(0, first_index);

    std::optional<size_t> best_end;
    int best_total = INF;
    //  Ending a leg where the player could have gone on.
    auto stop_cost = [&](const SearchState& s){
        if (!s.moving || stops_at(maps, s.player, (MapDirection)s.facing)){
            return 0;
        }
        return costs.per_open_stop;
    };
    auto arrival_cost = [&](const SearchState& s){
        int cost = stop_cost(s);
        if (goal.facing && s.facing != (int)*goal.facing){
            cost += costs.per_turn;
        }
        return cost;
    };

    while (!queue.empty()){
        const auto [cost, index] = queue.top();
        queue.pop();
        if (cost != distance_of(index)){
            continue;
        }
        if (cost >= best_total){
            break;
        }
        const SearchState current = states[index];
        if (current.player.map == goal_map && current.player.state.position == goal.position.point){
            const int total = cost + arrival_cost(current);
            if (total < best_total){
                best_total = total;
                best_end = index;
            }
            continue;
        }

        for (MapDirection direction : DIRECTIONS){
            const std::optional<Transition> transition = press(maps, current.player, direction);
            if (!transition){
                continue;
            }
            const bool continues = transition->action == RouteAction::WALK && !transition->warped && !transition->dismounted;
            SearchState next{transition->next, (int)direction, continues};
            int step_cost = costs.per_tile;
            if (current.facing != (int)direction){
                step_cost += costs.per_turn + stop_cost(current);
            }
            if (transition->action == RouteAction::SURF){
                step_cost += costs.per_surf;
            }
            if (transition->warped){
                step_cost += costs.per_warp;
            }
            if (maps[next.player.map].tall_grass(next.player.state.position)){
                step_cost += costs.per_grass_tile;
            }
            const Move move{index, direction, transition->action, transition->warped, transition->dismounted, transition->walked_out};
            const size_t next_index = index_of(next);
            if (cost + step_cost < distance_of(next_index)){
                distance[next_index] = cost + step_cost;
                came_from[next_index] = move;
                states[next_index] = next;
                queue.emplace(cost + step_cost, next_index);
            }
        }
    }

    if (!best_end){
        return std::nullopt;
    }

    //  Walk back to the start, then merge single moves into legs.
    std::vector<std::pair<Move, SearchState>> moves;
    for (size_t index = *best_end; index != first_index; index = came_from[index].from){
        moves.emplace_back(came_from[index], states[index]);
    }
    std::vector<RouteStep> route;
    bool can_extend = false;
    bool before_surfing = first.player.state.surfing;
    for (auto iter = moves.rbegin(); iter != moves.rend(); ++iter){
        const Move& move = iter->first;
        const SearchState& after = iter->second;
        const MapPosition end{maps[after.player.map].name(), after.player.state.position};
        if (can_extend && move.action == RouteAction::WALK && !move.dismounted
            && route.back().action == RouteAction::WALK && route.back().direction == move.direction
        ){
            route.back().tiles++;
            route.back().warps = move.warped;
            route.back().walked_out = move.walked_out;
            route.back().end = end;
        }else{
            route.push_back(RouteStep{move.action, move.direction, 1, move.warped, before_surfing, false, move.walked_out, end});
        }
        route.back().to_wall = move.action == RouteAction::WALK && !move.warped
            && stops_at(maps, after.player, move.direction);
        //  A warp or a Surf ends the leg: the game stops the player. Landing
        //  from the water ends it too, since walking is half the speed.
        can_extend = move.action == RouteAction::WALK && !move.warped && !move.dismounted;
        before_surfing = after.player.state.surfing;
    }
    const SearchState& last = states[*best_end];
    if (goal.facing && last.facing != (int)*goal.facing){
        route.push_back(RouteStep{RouteAction::FACE, *goal.facing, 0, false, false, false, false, goal.position});
    }
    return route;
}

MapPosition follow_overworld_route(
    const std::vector<OverworldMap>& maps,
    const MapPosition& start,
    const std::vector<RouteStep>& route
){
    Located player;
    player.map = find_map(maps, start.map);
    if (player.map < 0){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route from a map that isn't loaded: " + start.map);
    }
    player.state.position = start.point;
    player.state.elevation = maps[player.map].elevation(start.point);
    player.state.surfing = maps[player.map].water(start.point);

    for (const RouteStep& step : route){
        if (step.action == RouteAction::FACE){
            continue;
        }
        for (int c = 0; c < step.tiles; c++){
            const std::optional<Transition> transition = press(maps, player, step.direction);
            if (!transition || transition->action != step.action){
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    "Step refused at " + to_string(MapPosition{maps[player.map].name(), player.state.position}) + ": " + to_string(step)
                );
            }
            player = transition->next;
        }
        const MapPosition position{maps[player.map].name(), player.state.position};
        if (!(position == step.end)){
            throw InternalProgramError(
                nullptr, PA_CURRENT_FUNCTION,
                "Ended at " + to_string(position) + " instead of " + to_string(step.end)
            );
        }
    }
    return MapPosition{maps[player.map].name(), player.state.position};
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
const OverworldMap& small_map(){
    static const OverworldMap MAP(MapText{
        "Small",
        {
            "#####",
            "#.n.#",
            "#.~.#",
            "#####",
        },
        {
            "33333",
            "33403",
            "33133",
            "33333",
        },
        {},
        {},
    });
    return MAP;
}

UnitTestResult test_step_rules(){
    const OverworldMap& map = small_map();
    PlayerState state{{1, 1}, 3, false};
    if (classify_step(map, state, MapDirection::EAST) != StepKind::BLOCKED){
        return "Walked from elevation 3 onto elevation 4.";
    }
    if (classify_step(map, PlayerState{{2, 0}, 3, false}, MapDirection::SOUTH) != StepKind::BLOCKED){
        return "Entered a north-blocked tile moving south.";
    }
    state = PlayerState{{1, 2}, 3, false};
    if (classify_step(map, state, MapDirection::EAST) != StepKind::START_SURF){
        return "Can't start surfing onto the pond.";
    }
    state = after_step(map, state, MapDirection::EAST, StepKind::START_SURF);
    if (!state.surfing || state.elevation != 1){
        return "Surfing didn't put the player on the water.";
    }
    if (classify_step(map, state, MapDirection::EAST) != StepKind::STOP_SURFING){
        return "Can't land from the pond.";
    }
    if (classify_step(map, PlayerState{{3, 2}, 3, false}, MapDirection::NORTH) != StepKind::WALK){
        return "Can't step from elevation 3 onto the elevation-0 stair.";
    }
    return true;
}

const OverworldMap* map_named(const std::vector<OverworldMap>& maps, const std::string& name){
    const int index = find_map(maps, name);
    return index < 0 ? nullptr : &maps[index];
}

UnitTestResult test_maps(){
    const std::vector<OverworldMap> maps = overworld_maps(post_game_flags());
    const OverworldMap* b1f = map_named(maps, "CeruleanCave_B1F");
    const OverworldMap* city = map_named(maps, "CeruleanCity");
    if (b1f == nullptr || city == nullptr || map_named(maps, "CeruleanCity_PokemonCenter_1F") == nullptr){
        return "Missing a map.";
    }
    if (b1f->width() != 40 || b1f->height() != 23 || city->width() != 48 || city->height() != 40){
        return "Unexpected map size.";
    }
    const MapObject* mewtwo = b1f->object_at(CERULEAN_CAVE_MEWTWO);
    if (mewtwo == nullptr || mewtwo->name != "mewtwo"){
        return "Mewtwo is not on B1F at " + to_string(CERULEAN_CAVE_MEWTWO) + ".";
    }
    if (city->object_at({1, 13}) != nullptr){
        return "The cave guard is still there after the Hall of Fame.";
    }
    if (map_named(overworld_maps(), "CeruleanCity")->object_at({1, 13}) == nullptr){
        return "The cave guard is missing before the Hall of Fame.";
    }
    return true;
}

//  Plans `start` to `goal`, replays it with the step rules and checks where it ends.
UnitTestResult check_route(
    const std::vector<OverworldMap>& maps,
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

const RouteGoal FACING_MEWTWO{
    {"CeruleanCave_B1F", CERULEAN_CAVE_MEWTWO.next(MapDirection::SOUTH)}, MapDirection::NORTH
};

UnitTestResult test_entrance_to_mewtwo(){
    const std::vector<OverworldMap> maps = overworld_maps(post_game_flags());
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(maps, {"CeruleanCave_1F", CERULEAN_CAVE_ENTRANCE}, FACING_MEWTWO, route);
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
    const std::vector<OverworldMap> maps = overworld_maps(post_game_flags());
    std::vector<RouteStep> route;
    UnitTestResult result = check_route(
        maps, {"CeruleanCity_PokemonCenter_1F", CERULEAN_POKEMON_CENTER_PC}, FACING_MEWTWO, route
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
    const std::vector<OverworldMap> maps = overworld_maps();
    if (plan_overworld_route(maps, {"CeruleanCity_PokemonCenter_1F", CERULEAN_POKEMON_CENTER_PC}, FACING_MEWTWO)){
        return "Planned past the cave guard.";
    }
    return true;
}

UnitTestResult test_mewtwo_tile_unreachable(){
    const std::vector<OverworldMap> maps = overworld_maps(post_game_flags());
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        maps,
        {"CeruleanCave_1F", CERULEAN_CAVE_ENTRANCE},
        {{"CeruleanCave_B1F", CERULEAN_CAVE_MEWTWO}, std::nullopt}
    );
    if (route){
        return "Planned a route onto Mewtwo itself:" + describe(*route);
    }
    return true;
}

}

void add_tests_OverworldRoute(UnitTestDatabase& database){
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
