/*  Overworld Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_CeruleanCaveMaps.h"
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

//  Stepping onto a warp tile moves the player to the destination warp.
//  Returns false if the warp leads to a map that isn't loaded.
bool resolve_warp(const std::vector<OverworldMap>& maps, Located& player, bool& warped){
    warped = false;
    const MapWarp* warp = maps[player.map].warp_at(player.state.position);
    if (warp == nullptr){
        return true;
    }
    const int destination = find_map(maps, warp->dest_map);
    if (destination < 0 || warp->dest_warp < 0 || warp->dest_warp >= (int)maps[destination].warps().size()){
        return false;
    }
    const MapWarp& arrival = maps[destination].warps()[warp->dest_warp];
    player.map = destination;
    player.state.position = {arrival.x, arrival.y};
    player.state.elevation = maps[destination].elevation(player.state.position);
    player.state.surfing = false;
    warped = true;
    return true;
}

//  The search state: a player and the way they face. 4 stands for "not
//  known yet", before the first step.
struct SearchState{
    Located player;
    int facing = 4;
};

class StateIndex{
public:
    explicit StateIndex(const std::vector<OverworldMap>& maps){
        for (const OverworldMap& map : maps){
            m_width = std::max(m_width, map.width());
            m_height = std::max(m_height, map.height());
        }
        m_count = maps.size() * m_width * m_height * 16 * 2 * 5;
    }
    size_t count() const{
        return m_count;
    }
    size_t operator()(const SearchState& s) const{
        size_t index = s.player.map;
        index = index * m_height + s.player.state.position.y;
        index = index * m_width + s.player.state.position.x;
        index = index * 16 + s.player.state.elevation;
        index = index * 2 + (s.player.state.surfing ? 1 : 0);
        return index * 5 + s.facing;
    }

private:
    int m_width = 0;
    int m_height = 0;
    size_t m_count = 0;
};

//  One single-tile move of the search.
struct Move{
    size_t from = 0;
    MapDirection direction = MapDirection::NORTH;
    RouteAction action = RouteAction::WALK;
    bool warped = false;
    bool dismounted = false;
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
    std::vector<int> distance(index_of.count(), INF);
    std::vector<Move> came_from(index_of.count());
    std::vector<SearchState> states(index_of.count());

    SearchState first;
    first.player.map = start_map;
    first.player.state.position = start.point;
    first.player.state.elevation = maps[start_map].elevation(start.point);
    const size_t first_index = index_of(first);
    distance[first_index] = 0;
    states[first_index] = first;

    using Entry = std::pair<int, size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    queue.emplace(0, first_index);

    std::optional<size_t> best_end;
    int best_total = INF;
    auto facing_cost = [&](const SearchState& s){
        if (!goal.facing || s.facing == (int)*goal.facing){
            return 0;
        }
        return costs.per_turn;
    };

    while (!queue.empty()){
        const auto [cost, index] = queue.top();
        queue.pop();
        if (cost != distance[index]){
            continue;
        }
        if (cost >= best_total){
            break;
        }
        const SearchState current = states[index];
        if (current.player.map == goal_map && current.player.state.position == goal.position.point){
            const int total = cost + facing_cost(current);
            if (total < best_total){
                best_total = total;
                best_end = index;
            }
            continue;
        }

        const OverworldMap& map = maps[current.player.map];
        for (MapDirection direction : DIRECTIONS){
            const StepKind kind = classify_step(map, current.player.state, direction);
            if (kind == StepKind::BLOCKED){
                continue;
            }
            SearchState next = current;
            next.facing = (int)direction;
            next.player.state = after_step(map, current.player.state, direction, kind);
            int step_cost = costs.per_tile + (current.facing == (int)direction ? 0 : costs.per_turn);
            Move move{index, direction, RouteAction::WALK, false, kind == StepKind::STOP_SURFING};
            if (kind == StepKind::START_SURF){
                move.action = RouteAction::SURF;
                step_cost += costs.per_surf;
            }else if (!resolve_warp(maps, next.player, move.warped)){
                continue;
            }
            if (move.warped){
                step_cost += costs.per_warp;
            }
            const size_t next_index = index_of(next);
            if (cost + step_cost < distance[next_index]){
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
    for (auto iter = moves.rbegin(); iter != moves.rend(); ++iter){
        const Move& move = iter->first;
        const SearchState& after = iter->second;
        const MapPosition end{maps[after.player.map].name(), after.player.state.position};
        if (can_extend && move.action == RouteAction::WALK && !move.dismounted
            && route.back().action == RouteAction::WALK && route.back().direction == move.direction
        ){
            route.back().tiles++;
            route.back().warps = move.warped;
            route.back().end = end;
        }else{
            route.push_back(RouteStep{move.action, move.direction, 1, move.warped, end});
        }
        //  A warp or a Surf ends the leg: the game stops the player.
        can_extend = move.action == RouteAction::WALK && !move.warped;
    }
    const SearchState& last = states[*best_end];
    if (goal.facing && last.facing != (int)*goal.facing){
        route.push_back(RouteStep{RouteAction::FACE, *goal.facing, 0, false, goal.position});
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

    for (const RouteStep& step : route){
        if (step.action == RouteAction::FACE){
            continue;
        }
        for (int c = 0; c < step.tiles; c++){
            const OverworldMap& map = maps[player.map];
            const StepKind kind = classify_step(map, player.state, step.direction);
            const bool allowed = step.action == RouteAction::SURF
                ? kind == StepKind::START_SURF
                : kind == StepKind::WALK || kind == StepKind::STOP_SURFING;
            if (!allowed){
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    "Step refused at " + to_string(MapPosition{map.name(), player.state.position}) + ": " + to_string(step)
                );
            }
            player.state = after_step(map, player.state, step.direction, kind);
            bool warped = false;
            if (step.action == RouteAction::WALK && !resolve_warp(maps, player, warped)){
                throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Warp off the loaded maps: " + to_string(step));
            }
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
        return m_body();
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

UnitTestResult test_cerulean_cave_maps(){
    const std::vector<OverworldMap>& maps = cerulean_cave_maps();
    if (maps.size() != 3){
        return "Expected 3 floors.";
    }
    for (const OverworldMap& map : maps){
        if (map.width() != 40 || map.height() != 23){
            return map.name() + " is not 40x23.";
        }
    }
    const MapObject* mewtwo = maps[2].object_at(CERULEAN_CAVE_MEWTWO);
    if (maps[2].name() != "CeruleanCave_B1F" || mewtwo == nullptr || mewtwo->name != "mewtwo"){
        return "Mewtwo is not on B1F at " + to_string(CERULEAN_CAVE_MEWTWO) + ".";
    }
    return true;
}

UnitTestResult test_entrance_to_mewtwo(){
    const std::vector<OverworldMap>& maps = cerulean_cave_maps();
    const MapPosition start{"CeruleanCave_1F", CERULEAN_CAVE_ENTRANCE};
    const RouteGoal goal{{"CeruleanCave_B1F", CERULEAN_CAVE_MEWTWO.next(MapDirection::SOUTH)}, MapDirection::NORTH};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, goal);
    if (!route){
        return "No route from the entrance to Mewtwo.";
    }
    const MapPosition end = follow_overworld_route(maps, start, *route);
    if (!(end == goal.position)){
        return "Route ends at " + to_string(end) + ":" + describe(*route);
    }
    bool surfs = false;
    bool reaches_b1f = false;
    for (const RouteStep& step : *route){
        surfs |= step.action == RouteAction::SURF;
        reaches_b1f |= step.warps && step.end.map == "CeruleanCave_B1F";
    }
    if (!surfs || !reaches_b1f){
        return "Expected to take the ladder to B1F and to surf:" + describe(*route);
    }
    const RouteStep& last = route->back();
    if (last.direction != MapDirection::NORTH){
        return "Doesn't end facing Mewtwo:" + describe(*route);
    }
    return true;
}

UnitTestResult test_mewtwo_tile_unreachable(){
    const std::vector<OverworldMap>& maps = cerulean_cave_maps();
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
    database.add<Test_OverworldRoute>("Cerulean Cave maps", test_cerulean_cave_maps);
    database.add<Test_OverworldRoute>("Cerulean Cave entrance to Mewtwo", test_entrance_to_mewtwo);
    database.add<Test_OverworldRoute>("Mewtwo's tile is blocked", test_mewtwo_tile_unreachable);
}



}
}
}
