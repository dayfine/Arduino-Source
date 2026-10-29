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
#include "Pokemon/Navigation/Pokemon_MapSet.h"
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
        if (cost != distance_of(index)){
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

        for (MapDirection direction : DIRECTIONS){
            const std::optional<Transition> transition = press(maps, current.player, direction);
            if (!transition){
                continue;
            }
            SearchState next{transition->next, (int)direction};
            int step_cost = costs.per_tile + (current.facing == (int)direction ? 0 : costs.per_turn);
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
    bool before_surfing = false;
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



namespace{

Pokemon::GridDirection to_grid(MapDirection direction){
    switch (direction){
    case MapDirection::NORTH:
        return Pokemon::GridDirection::NORTH;
    case MapDirection::EAST:
        return Pokemon::GridDirection::EAST;
    case MapDirection::SOUTH:
        return Pokemon::GridDirection::SOUTH;
    case MapDirection::WEST:
        return Pokemon::GridDirection::WEST;
    }
    return Pokemon::GridDirection::NORTH;
}
MapDirection from_grid(Pokemon::GridDirection direction){
    switch (direction){
    case Pokemon::GridDirection::NORTH:
        return MapDirection::NORTH;
    case Pokemon::GridDirection::EAST:
        return MapDirection::EAST;
    case Pokemon::GridDirection::SOUTH:
        return MapDirection::SOUTH;
    case Pokemon::GridDirection::WEST:
        return MapDirection::WEST;
    }
    return MapDirection::NORTH;
}

//  The tiles a plain walk in `state` can stand on, as a tile grid: floor at
//  the player's elevation (or any, on elevation 0), or water while surfing.
//  Tiles that warp when stepped on and objects are walls, so a leg never
//  takes a ladder or cave mouth by accident. Exit mats are floor: they only
//  warp a player walking off them in their arrow's direction.
Pokemon::TileGrid walk_grid(const OverworldMap& map, const PlayerState& state){
    std::vector<std::string> rows;
    for (int y = 0; y < map.height(); y++){
        std::string row;
        for (int x = 0; x < map.width(); x++){
            const MapPoint point{x, y};
            const uint8_t level = map.elevation(point);
            bool standable = !map.wall(point)
                && map.object_at(point) == nullptr
                && !(map.warps_on_step(point) && map.warp_at(point) != nullptr);
            if (state.surfing){
                standable = standable && map.water(point);
            }else{
                standable = standable && !map.water(point)
                    && (state.elevation == 0 || level == state.elevation || level == 0 || level == 15);
            }
            char tile = '#';
            if (standable){
                tile = map.tall_grass(point) ? '"' : '.';
            }
            row += tile;
        }
        rows.push_back(row);
    }
    return Pokemon::TileGrid(rows);
}

//  `start` and the maps joined to it by connections, placed on one grid.
Pokemon::MapSet walk_area(const std::vector<OverworldMap>& maps, int start, const PlayerState& state){
    Pokemon::MapSet area;
    std::vector<std::pair<int, Pokemon::TilePos>> pending{{start, {0, 0}}};
    std::vector<bool> placed(maps.size(), false);
    while (!pending.empty()){
        const auto [index, offset] = pending.back();
        pending.pop_back();
        if (placed[index]){
            continue;
        }
        placed[index] = true;
        const OverworldMap& map = maps[index];
        area.add(map.name(), walk_grid(map, state), offset);
        for (const MapConnection& connection : map.connections()){
            const int next = find_map(maps, connection.map);
            if (next < 0 || placed[next]){
                continue;
            }
            const OverworldMap& other = maps[next];
            Pokemon::TilePos at = offset;
            switch (connection.direction){
            case MapDirection::NORTH:
                at = {offset.x + connection.offset, offset.y - other.height()};
                break;
            case MapDirection::SOUTH:
                at = {offset.x + connection.offset, offset.y + map.height()};
                break;
            case MapDirection::WEST:
                at = {offset.x - other.width(), offset.y + connection.offset};
                break;
            case MapDirection::EAST:
                at = {offset.x + map.width(), offset.y + connection.offset};
                break;
            }
            pending.emplace_back(next, at);
        }
    }
    return area;
}

//  How far an open leg may end from where it was timed to. FRLG moves a whole
//  tile per 16 frames and finishes a step once started, so a short leg released
//  half a tile early is exact; only a long one drifts, from the difference
//  between the assumed and the real step time.
Pokemon::RobustPlanCosts frlg_leg_error(){
    Pokemon::RobustPlanCosts costs;
    costs.open_error = 0;
    costs.open_error_per = 6;
    return costs;
}

//  The maps a run of plain walking can cross, joined on one grid with a border
//  of walls: nothing off the loaded maps can be walked on, so an edge stops the
//  player like any other wall.
class WalkArea{
public:
    WalkArea(const std::vector<OverworldMap>& maps, const Located& from)
        : m_maps(maps)
        , m_set(walk_area(maps, from.map, from.state))
        , m_grid(padded(m_set.world()))
        , m_state(from.state)
    {}

    const Pokemon::TileGrid& grid() const{ return m_grid; }
    Pokemon::TileGrid& grid(){ return m_grid; }

    Pokemon::TilePos to_grid(const Located& player) const{
        const Pokemon::TilePos world = m_set.to_world(
            m_maps[player.map].name(), {player.state.position.x, player.state.position.y}
        );
        return {world.x + 1, world.y + 1};
    }
    std::optional<Located> to_player(const Pokemon::TilePos& tile) const{
        const std::optional<Pokemon::MapTile> local = m_set.to_local({tile.x - 1, tile.y - 1});
        if (!local){
            return std::nullopt;
        }
        Located player;
        player.map = find_map(m_maps, local->map);
        player.state = m_state;
        player.state.position = {local->pos.x, local->pos.y};
        const uint8_t level = m_maps[player.map].elevation(player.state.position);
        if (level != 15){
            player.state.elevation = level;
        }
        return player;
    }

private:
    static Pokemon::TileGrid padded(const Pokemon::TileGrid& world){
        std::vector<std::string> rows = world.rows();
        const size_t width = rows.empty() ? 0 : rows[0].size();
        for (std::string& row : rows){
            for (char& tile : row){
                if (tile == '?'){
                    tile = '#';
                }
            }
            row = "#" + row + "#";
        }
        rows.insert(rows.begin(), std::string(width + 2, '#'));
        rows.push_back(std::string(width + 2, '#'));
        return Pokemon::TileGrid(rows);
    }

    const std::vector<OverworldMap>& m_maps;
    Pokemon::MapSet m_set;
    Pokemon::TileGrid m_grid;
    PlayerState m_state;
};

bool same_place(const Located& a, const Located& b){
    return a.map == b.map && a.state.position == b.state.position;
}
void add_unique(std::vector<Located>& set, const Located& player){
    for (const Located& p : set){
        if (same_place(p, player)){
            return;
        }
    }
    set.push_back(player);
}

//  One tile of the planned route, as a press of the D-pad.
struct TileMove{
    MapDirection direction;
    RouteAction action;
    Transition transition;
    Located before;
};

//  What the route does after a run of plain walking.
struct NextAction{
    enum Kind{
        END,        //  stand on the goal
        WARP,       //  walk on until a warp (ladder, cave mouth, exit mat, door)
        SURF,       //  start surfing
        DISMOUNT,   //  step off the water onto land
    } kind = END;
    TileMove move;
};

//  Where `player` ends up doing `action`, or nothing if it doesn't do the same
//  thing from there. WARP walks up to `MAX_WARP_WALK` tiles.
constexpr int MAX_WARP_WALK = 16;
std::optional<std::pair<Located, int>> try_action(
    const std::vector<OverworldMap>& maps, const Located& player, const NextAction& action
){
    switch (action.kind){
    case NextAction::END:
        return std::make_pair(player, 0);
    case NextAction::SURF:
    case NextAction::DISMOUNT:{
        const std::optional<Transition> t = press(maps, player, action.move.direction);
        if (!t || t->action != action.move.action || t->dismounted != action.move.transition.dismounted || t->warped){
            return std::nullopt;
        }
        return std::make_pair(t->next, 1);
    }
    case NextAction::WARP:{
        Located at = player;
        for (int c = 1; c <= MAX_WARP_WALK; c++){
            const std::optional<Transition> t = press(maps, at, action.move.direction);
            if (!t || t->action != RouteAction::WALK){
                return std::nullopt;
            }
            if (t->warped){
                if (!same_place(t->next, action.move.transition.next)){
                    return std::nullopt;
                }
                return std::make_pair(t->next, c);
            }
            at = t->next;
        }
        return std::nullopt;
    }
    }
    return std::nullopt;
}

//  The single-tile moves of `route` from `start`.
std::vector<TileMove> to_moves(const std::vector<OverworldMap>& maps, const Located& start, const std::vector<RouteStep>& route){
    std::vector<TileMove> moves;
    Located player = start;
    for (const RouteStep& step : route){
        for (int c = 0; c < step.tiles; c++){
            const std::optional<Transition> transition = press(maps, player, step.direction);
            if (!transition){
                throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route step refused: " + to_string(step));
            }
            moves.push_back(TileMove{step.direction, transition->action, *transition, player});
            player = transition->next;
        }
    }
    return moves;
}

}



std::vector<RouteStep> anchor_overworld_route(
    const std::vector<OverworldMap>& maps,
    const MapPosition& start,
    const std::vector<RouteStep>& route
){
    Located first;
    first.map = find_map(maps, start.map);
    first.state.position = start.point;
    first.state.elevation = maps[first.map].elevation(start.point);
    const std::vector<TileMove> moves = to_moves(maps, first, route);

    std::vector<RouteStep> anchored;
    std::vector<Located> possible{first};
    size_t index = 0;
    while (true){
        //  A run of plain walking, then the action that ends it.
        const size_t run_begin = index;
        while (index < moves.size()
            && moves[index].action == RouteAction::WALK
            && !moves[index].transition.warped
            && !moves[index].transition.dismounted
        ){
            index++;
        }
        NextAction next;
        Located goal = index < moves.size() ? moves[index].before : (moves.empty() ? first : moves.back().transition.next);
        if (index < moves.size()){
            next.move = moves[index];
            next.kind = next.move.transition.warped ? NextAction::WARP
                : next.move.action == RouteAction::SURF ? NextAction::SURF
                : NextAction::DISMOUNT;
            if (next.kind == NextAction::WARP){
                //  Walk on to the warp from wherever the straight walk to it starts.
                size_t c = index;
                while (c > run_begin && moves[c - 1].direction == next.move.direction){
                    c--;
                }
                goal = moves[c].before;
            }
        }

        //  The tiles the run may end on: any the next action works from.
        WalkArea area(maps, possible.front());
        auto works = [&](const Pokemon::TilePos& tile){
            const std::optional<Located> player = area.to_player(tile);
            if (!player){
                return false;
            }
            if (next.kind == NextAction::END){
                return same_place(*player, goal);
            }
            return try_action(maps, *player, next).has_value();
        };
        std::vector<Pokemon::TilePos> from;
        for (const Located& p : possible){
            const Pokemon::TilePos tile = area.to_grid(p);
            area.grid().set(tile, Pokemon::TileType::WALKABLE);
            from.push_back(tile);
        }

        std::vector<Located> after;
        bool all_work = true;
        for (const Pokemon::TilePos& tile : from){
            all_work = all_work && works(tile);
        }
        if (all_work){
            after = possible;
        }else{
            const std::optional<std::vector<Pokemon::PlannedLeg>> legs = Pokemon::plan_robust_route(area.grid(), from, works, frlg_leg_error());
            if (!legs){
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    "No walk that survives a tile of error to the next action from " + to_string(MapPosition{maps[possible.front().map].name(), possible.front().state.position})
                );
            }
            for (const Pokemon::PlannedLeg& planned : *legs){
                RouteStep step;
                step.direction = from_grid(planned.leg.direction);
                step.tiles = (int)planned.leg.tiles;
                step.to_wall = planned.leg.ends_at_wall;
                const std::optional<Located> end = area.to_player(planned.after.front());
                step.end = MapPosition{maps[end->map].name(), end->state.position};
                anchored.push_back(step);
            }
            for (const Pokemon::TilePos& tile : legs->back().after){
                add_unique(after, *area.to_player(tile));
            }
        }

        if (next.kind == NextAction::END){
            if (after.size() != 1 || !same_place(after[0], goal)){
                throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "The anchored route doesn't end on one tile.");
            }
            return anchored;
        }

        //  The next action, from every tile the player may be on.
        std::vector<Located> landed;
        int longest = 0;
        for (const Located& p : after){
            const std::optional<std::pair<Located, int>> result = try_action(maps, p, next);
            add_unique(landed, result->first);
            longest = std::max(longest, result->second);
        }
        RouteStep step;
        step.action = next.kind == NextAction::SURF ? RouteAction::SURF : RouteAction::WALK;
        step.direction = next.move.direction;
        step.tiles = longest;
        step.warps = next.kind == NextAction::WARP;
        step.end = MapPosition{maps[landed.front().map].name(), landed.front().state.position};
        anchored.push_back(step);
        possible = landed;
        index++;
    }
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

UnitTestResult test_anchored_route(){
    const std::vector<OverworldMap> maps = overworld_maps(post_game_flags());
    const MapPosition start{"CeruleanCity_PokemonCenter_1F", CERULEAN_POKEMON_CENTER_PC};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, start, FACING_MEWTWO);
    if (!route){
        return "No route.";
    }
    //  Throws if any run can't be walked to its next action allowing for a
    //  tile of error, or if the route doesn't end on the goal.
    const std::vector<RouteStep> anchored = anchor_overworld_route(maps, start, *route);
    if (!(anchored.back().end == FACING_MEWTWO.position)){
        return "Anchored route ends at " + to_string(anchored.back().end) + ":" + describe(anchored);
    }
    size_t to_wall = 0;
    size_t longest_open = 0;
    for (const RouteStep& step : anchored){
        if (step.action != RouteAction::WALK){
            continue;
        }
        if (step.to_wall){
            to_wall++;
        }else if (!step.warps){
            longest_open = std::max(longest_open, (size_t)step.tiles);
        }
    }
    if (to_wall == 0 || longest_open > 12){
        return "Expected legs to walls and no open leg over 12 tiles:" + describe(anchored);
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
