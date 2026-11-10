/*  Overworld Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <ranges>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_LegPlanner.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldSpin.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


const RouteCosts& default_route_costs(){
    static const RouteCosts COSTS{
        {RouteCost::TILE, 1},
        {RouteCost::TURN, 2},
        {RouteCost::WARP, 30},
        {RouteCost::SURF, 40},
        {RouteCost::GRASS_TILE, 20},
        {RouteCost::OPEN_STOP, 8},
        {RouteCost::OPEN_WILD_STOP, 300},
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
    //  The step landed on a spinner, which slid the player on: how far,
    //  and the way they last moved.
    std::optional<SpinSlide> spin;
};

//  The player taking `warp`, or nothing if it leads off the loaded maps.
std::optional<Transition> take_warp(const std::vector<MapData>& maps, const MapWarp* warp){
    if (warp == nullptr || !warp->to){
        return std::nullopt;
    }
    const int destination = find_map(maps, warp->to->map);
    if (destination < 0){
        return std::nullopt;
    }
    const MapData& map = maps[destination];
    const auto arrival = map.points.find(warp->to->point);
    if (arrival == map.points.end()){
        return std::nullopt;
    }
    TilePos position = arrival->second;
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
        if (connection.side != direction){
            continue;
        }
        const int destination = find_map(maps, connection.to_map);
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
    if (transition.action == RouteAction::WALK && spin_direction(map, landed)){
        transition.spin = spin_slide(map, transition.next.state);
        if (!transition.spin){
            return std::nullopt;
        }
        transition.next.state = transition.spin->end;
    }
    return transition;
}

//  Whether pressing `direction` takes a warp to a map that isn't loaded,
//  which press() refuses as if it were a wall.
bool warps_off_loaded_maps(const std::vector<MapData>& maps, const Located& player, Direction direction){
    const MapData& map = maps[player.map];
    const TilePos here = player.state.position;
    const TilePos ahead = here.next(direction);
    const MapWarp* warp = nullptr;
    if (exit_mat(map, here) == direction){
        warp = warp_at(map, here);
    }else if (direction == Direction::NORTH && is_door(map, ahead) && !player.state.surfing){
        warp = warp_at(map, ahead);
    }else if (in_bounds(map, ahead) && warps_on_step(map, ahead) && classify_step(map, player.state, direction) != StepKind::BLOCKED){
        warp = warp_at(map, ahead);
    }
    return warp != nullptr && !take_warp(maps, warp);
}

//  Whether holding `direction` stops the player where they are: a wall, or
//  water they'd need to Surf onto. A leg that ends there can be
//  held for longer than it takes, and walked again after an interruption. A
//  person in the way doesn't count, since they may have wandered off, and
//  neither does an edge to a map that isn't loaded.
bool stops_at(const std::vector<MapData>& maps, const Located& player, Direction direction){
    const std::optional<Transition> transition = press(maps, player, direction);
    if (transition){
        return transition->action == RouteAction::SURF;
    }
    //  Nor does a warp to a map that isn't loaded: the player takes it (console
    //  B, 2026-11-02, "west 5 (to wall)" rode Route 4's Pokemon Center
    //  escalator up to 2F).
    if (warps_off_loaded_maps(maps, player, direction)){
        return false;
    }
    const MapData& map = maps[player.map];
    const TilePos ahead = player.state.position.next(direction);
    //  Held into a sign moving north or south, the game shows the sign's
    //  message: the player stops, but the walk is interrupted (console B,
    //  2026-11-01, the poster on the bedroom's wall).
    if (tile_at(map, ahead) == TileKind::SIGN && (direction == Direction::NORTH || direction == Direction::SOUTH)){
        return false;
    }
    //  Held into a ledge the way it faces, the player hops down it (console
    //  B, 2026-11-01, Viridian City: "south 2 (to wall)" from the Pokemon
    //  Center hopped the ledge two rows down). Routes don't hop ledges, so
    //  none is held into.
    if (is_ledge(map, ahead)){
        return false;
    }
    if (in_bounds(map, ahead)){
        return object_at(map, ahead) == nullptr;
    }
    for (const MapConnection& connection : map.connections){
        if (connection.side == direction){
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
        if (!transition || (transition->action == RouteAction::SURF && route_cost(m_costs, RouteCost::SURF) < 0)){
            return std::nullopt;
        }
        Pokemon::Step<State> step;
        step.next = transition->next;
        step.action = transition->action == RouteAction::SURF ? ACTION_SURF : 0;
        //  Leaving the water: the player walks at half the speed from here.
        step.starts_leg = transition->dismounted;
        //  A slide ends the leg: the hold must let go on the spinner.
        step.ends_leg = transition->warped || transition->dismounted || transition->spin;
        step.warped = transition->warped;
        step.facing = transition->walked_out ? Direction::SOUTH : direction;
        if (transition->spin){
            step.facing = transition->spin->facing;
            step.extra_cost += transition->spin->tiles * route_cost(m_costs, RouteCost::TILE);
        }
        if (transition->action == RouteAction::SURF){
            step.extra_cost += route_cost(m_costs, RouteCost::SURF);
        }
        if (transition->warped){
            step.extra_cost += route_cost(m_costs, RouteCost::WARP);
        }
        if (is_tall_grass(m_maps[step.next.map], step.next.state.position)){
            step.extra_cost += route_cost(m_costs, RouteCost::GRASS_TILE);
            step.wild = true;
        }
        if (step.next.state.surfing){
            step.wild = true;
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
    leg_costs.per_open_wild_stop = route_cost(costs, RouteCost::OPEN_WILD_STOP);
    return leg_costs;
}

//  The tiles the slide at the end of WALK `leg` takes, if it ends on a
//  spinner: only a leg's last step can slide (StepModel ends the leg).
std::optional<int> last_spin_tiles(const std::vector<MapData>& maps, const Pokemon::Leg<Located>& leg){
    Located player = leg.start;
    std::optional<Transition> transition;
    for (int c = 0; c < leg.tiles; c++){
        transition = press(maps, player, leg.direction);
        if (!transition){
            return std::nullopt;
        }
        player = transition->next;
    }
    if (!transition || !transition->spin){
        return std::nullopt;
    }
    return transition->spin->tiles;
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
    step.walk_only = !maps[leg.start.map].allow_running;
    step.to_wall = leg.to_wall;
    step.facing_after = leg.facing_after;
    step.end = MapPosition{maps[leg.end.map].name, leg.end.state.position};
    if (leg.kind != Pokemon::LegKind::FACE){
        //  Tall grass on any map the leg crosses, or the water. Counted on the
        //  start map only, "north 22" from Pallet Town through Route 1's grass
        //  wasn't watched for battles (console B, 2026-11-01).
        step.wild = leg.wild || leg.start.state.surfing || leg.kind == Pokemon::LegKind::ACTION;
    }
    if (leg.kind == Pokemon::LegKind::WALK){
        step.spin_tiles = last_spin_tiles(maps, leg);
        step.to_wall = step.to_wall && !step.spin_tiles;
    }
    const std::vector<MapPosition> tiles = leg_tiles(maps, MapPosition{maps[leg.start.map].name, leg.start.state.position}, step);
    for (int c : std::views::iota(1, (int)tiles.size()) | std::views::filter([&](int n){ return tiles[n].map != tiles[n - 1].map; })){
        step.connection_steps.push_back(c);
    }
    return step;
}

} // namespace



std::optional<OverworldPress> predict_overworld_press(
    const std::vector<MapData>& maps, const OverworldPose& from, Direction direction
){
    const int map = find_map(maps, from.position.map);
    if (map < 0){
        return std::nullopt;
    }
    const Located start{map, {from.position.point, from.elevation, from.surfing}};
    const auto next = press(maps, start, direction);
    if (!next){
        return std::nullopt;
    }
    return OverworldPress{
        .pose = {
            .position = {maps[next->next.map].name, next->next.state.position},
            .elevation = next->next.state.elevation,
            .surfing = next->next.state.surfing,
            .facing = next->spin ? next->spin->facing : next->walked_out ? Direction::SOUTH : direction,
        },
        .warped = next->warped,
    };
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


namespace{

//  Further than any straight line on a map.
constexpr int MAX_HOLD_TILES = 128;

std::optional<Located> locate(const std::vector<MapData>& maps, const MapPosition& position, bool surfing){
    const int map = find_map(maps, position.map);
    if (map < 0){
        return std::nullopt;
    }
    Located player;
    player.map = map;
    player.state = PlayerState{position.point, elevation_at(maps[map], position.point), surfing};
    return player;
}

//  A plain step: no warp, no Surf, no getting off the water.
std::optional<Located> plain_step(const std::vector<MapData>& maps, const Located& player, Direction direction){
    const std::optional<Transition> transition = press(maps, player, direction);
    if (!transition || transition->warped || transition->dismounted || transition->spin || transition->action != RouteAction::WALK){
        return std::nullopt;
    }
    return transition->next;
}

} // namespace


namespace{

//  Holding north into a ledge stops the player: the maps don't keep which way
//  a ledge faces, but FRLG's face south, east or west (pokefirered has no
//  MB_JUMP_NORTH on any map these load). Only finding the player again
//  relies on this; routes never hold into a ledge (stops_at()).
bool ledge_stops_north(const std::vector<MapData>& maps, const Located& player, Direction direction){
    return direction == Direction::NORTH && is_ledge(maps[player.map], player.state.position.next(direction));
}

} // namespace

std::optional<WallHold> hold_to_wall(
    const std::vector<MapData>& maps,
    const MapPosition& from, bool surfing, Direction direction
){
    std::optional<Located> player = locate(maps, from, surfing);
    WallHold hold;
    hold.walk_only = player && !maps[player->map].allow_running;
    for (int c = 0; player && c <= MAX_HOLD_TILES; c++){
        if (stops_at(maps, *player, direction) || ledge_stops_north(maps, *player, direction)){
            hold.end = MapPosition{maps[player->map].name, player->state.position};
            hold.tiles = c;
            return hold;
        }
        player = plain_step(maps, *player, direction);
        if (player){
            const MapData& map = maps[player->map];
            hold.wild = hold.wild || is_tall_grass(map, player->state.position) || player->state.surfing;
        }
    }
    return std::nullopt;
}

std::vector<MapPosition> leg_tiles(
    const std::vector<MapData>& maps,
    const MapPosition& start, const RouteStep& step
){
    std::vector<MapPosition> tiles{start};
    std::optional<Located> player = locate(maps, start, step.surfing);
    const int along = step.tiles - (step.warps || step.spin_tiles ? 1 : 0);
    for (int c = 0; player && c < along; c++){
        player = plain_step(maps, *player, step.direction);
        if (player){
            tiles.push_back(MapPosition{maps[player->map].name, player->state.position});
        }
    }
    return tiles;
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
