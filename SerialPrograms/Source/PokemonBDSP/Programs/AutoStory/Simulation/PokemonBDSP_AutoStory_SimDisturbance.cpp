/*  BDSP AutoStory - Disturbances on the grid simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LegBattle.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_SimDisturbance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TileKind;
using Pokemon::TilePos;


namespace{

//  The caves on the story's way, where every tile has wild Pokemon.
constexpr std::string_view CAVE_MAPS[] = {"OreburghMineB1F", "OreburghMine", "RavagedPath"};
//  Drift comes in steps of this many thousandths of a tile: 4 ms of stick
//  across, 6 ms up or down, so the grid sim puts it back on the grid exactly.
constexpr int DRIFT_STEP = 25;

bool across(Direction direction){
    return direction == Direction::EAST || direction == Direction::WEST;
}
//  The player's offset from their nearest tile along an axis.
int offset_of(const SimPos& pos, bool along_x){
    const TilePos tile = nearest_tile(pos);
    return along_x ? pos.x - tile.x * SIM_TILE : pos.y - tile.y * SIM_TILE;
}
int along(const SimPos& pos, Direction direction){
    return across(direction) ? pos.x : pos.y;
}
bool wanderer_on(const GridSim& sim, const TilePos& pos){
    const std::vector<SimWanderer>& people = detail::sim_wanderers(sim);
    return std::any_of(people.begin(), people.end(), [&](const SimWanderer& person){ return person.pos == pos; });
}
//  Whether the player can drift a little toward `direction`: the tile there
//  is open ground with nobody on it, not a wall or a door.
bool room_toward(const GridSim& sim, Direction direction){
    const TilePos next = nearest_tile(sim.player().pos).next(direction);
    const std::optional<TileKind> tile = sim.world().tile(next);
    return tile && *tile != TileKind::DOOR && standable(sim.world(), next) && !wanderer_on(sim, next);
}
//  A hold that stopped against what is in its way, not against a person.
bool stopped_at_wall(const GridSim& sim, const SimMove& move, Direction direction){
    return move.stop == SimStop::BLOCKED && !wanderer_on(sim, nearest_tile(sim.player().pos).next(direction));
}

bool in_cave(const StoryWorld& world, const TilePos& pos){
    const std::optional<MapTile> tile = world.to_local(pos);
    return tile && std::ranges::any_of(CAVE_MAPS, [&](std::string_view map){ return tile->map == story_banner_name(std::string(map)); });
}

//  Whether `box` of `map` holds `pos`.
bool holds(const Pokemon::TileBox& box, const TilePos& pos){
    return box.x0 <= pos.x && pos.x <= box.x1 && box.y0 <= pos.y && pos.y <= box.y1;
}

WalkStop walk_stop(SimStop stop){
    switch (stop){
    case SimStop::BLOCKED:
        return WalkStop::BLOCKED;
    case SimStop::DIALOG:
        return WalkStop::DIALOG;
    case SimStop::WARP:
        return WalkStop::TRANSITION;
    case SimStop::DONE:
    case SimStop::LIMIT:
        return WalkStop::LIMIT;
    }
    return WalkStop::LIMIT;
}

} // namespace


SimDisturbance::SimDisturbance(Pokemon::DisturbancePlan plan)
    : m_plan(std::move(plan))
{}

bool SimDisturbance::battle_at(const StoryWorld& world, const TilePos& pos){
    const std::optional<TileKind> tile = world.tile(pos);
    if (!tile){
        return false;
    }
    const bool grass = *tile == TileKind::TALL_GRASS;
    const bool cave = !grass && in_cave(world, pos);
    m_tiles.grass += grass ? 1 : 0;
    m_tiles.cave += cave ? 1 : 0;
    const double rate = grass ? m_plan.rates.grass_encounter : cave ? m_plan.rates.cave_encounter : 0;
    if (rate <= 0 || !m_plan.battles.chance(rate)){
        return false;
    }
    m_battles++;
    return true;
}

void SimDisturbance::reset(){
    m_x = {};
    m_y = {};
}

//  The grid sim's position is the truth: a wall it stopped at, or a place
//  the executor put the player, changes the offset, and the drift with it.
void SimDisturbance::sync(const GridSim& sim){
    for (bool along_x : {true, false}){
        Axis& axis = along_x ? m_x : m_y;
        const int actual = offset_of(sim.player().pos, along_x);
        if (actual != axis.applied){
            axis.drift = actual;
            axis.applied = actual;
        }
    }
}

void SimDisturbance::walked(GridSim& sim, Direction direction, int thousandths, bool at_wall, int drift_per_tile){
    sync(sim);
    Axis& axis = across(direction) ? m_x : m_y;
    if (at_wall){
        axis.drift = axis.applied;
        return;
    }
    //  In steps of DRIFT_STEP, which a stick press moves exactly.
    const int64_t most = drift_per_tile / DRIFT_STEP;
    const int64_t steps = m_plan.drift.between(-most, most) * thousandths / SIM_TILE;
    axis.drift = std::clamp(axis.drift + (int)steps * DRIFT_STEP, -SIM_TILE + 100, SIM_TILE - 100);
    materialize(sim, across(direction));
}

//  Puts the drift past `fit` on the grid sim's position, with a stick press
//  that far, and turns back the way the player faced. A wall or a door in
//  the way absorbs it.
void SimDisturbance::materialize(GridSim& sim, bool along_x){
    Axis& axis = along_x ? m_x : m_y;
    const int target = std::abs(axis.drift) > m_plan.rates.fit ? axis.drift : 0;
    const int delta = target - axis.applied;
    if (delta == 0){
        return;
    }
    const Direction direction = along_x ? (delta > 0 ? Direction::EAST : Direction::WEST)
        : (delta > 0 ? Direction::SOUTH : Direction::NORTH);
    if (!room_toward(sim, direction)){
        axis.drift = axis.applied;
        return;
    }
    const Direction facing = sim.player().facing;
    const SimPos before = sim.player().pos;
    const Milliseconds tile_time = route_ms_per_tile(direction, MoveSpeed::WALK);
    sim.tap(direction, Milliseconds((std::abs(delta) * tile_time.count() + SIM_TILE / 2) / SIM_TILE), true);
    const int moved = along(sim.player().pos, direction) - along(before, direction);
    axis.applied += moved;
    if (std::abs(moved - delta) >= DRIFT_STEP){
        axis.drift = axis.applied;
    }
    sim.tap(facing, ROUTE_TURN_TAP, false);
}

//  The maps' wanderers on `world`, found from its wandering tiles: each
//  tile's map, and who on that map wanders over it.
const std::vector<SimWanderer>& SimDisturbance::people(const StoryWorld& world){
    auto found = m_people.find(&world);
    if (found != m_people.end()){
        return found->second;
    }
    std::vector<SimWanderer>& people = m_people[&world];
    for (const TilePos& pos : wandering_hazards(world)){
        const std::optional<MapTile> tile = world.to_local(pos);
        for (const auto& [name, json] : bdsp_map_json()){
            if (!tile || story_banner_name(name) != tile->map){
                continue;
            }
            add_people(people, name, {pos.x - tile->pos.x, pos.y - tile->pos.y}, tile->pos);
        }
    }
    return people;
}

//  Who on `map` (its tile (0, 0) at world tile `corner`) wanders over its
//  tile `local`, if not in `people` yet.
void SimDisturbance::add_people(std::vector<SimWanderer>& people, const std::string& map, const TilePos& corner, const TilePos& local){
    for (const Pokemon::MapObject& object : story_map(map).objects){
        const std::string name = map + "/" + object.name;
        const bool known = std::ranges::any_of(people, [&](const SimWanderer& person){ return person.name == name; });
        if (!object.wander || object.hide_flag || known || !holds(*object.wander, local)){
            continue;
        }
        const Pokemon::TileBox& box = *object.wander;
        people.push_back({.name = name, .box = {corner.x + box.x0, corner.y + box.y0, corner.x + box.x1, corner.y + box.y1},
            .pos = {corner.x + object.x, corner.y + object.y}});
    }
}

std::vector<SimWanderer> SimDisturbance::wanderers(const StoryWorld& world){
    std::vector<SimWanderer> out = people(world);
    for (SimWanderer& person : out){
        person.seed = m_plan.wanderers.next();
        person.time_offset = Milliseconds(m_plan.wanderers.between(0, m_plan.rates.wanderer_start.count()));
        person.step_interval = m_plan.rates.wander_step;
    }
    return out;
}


DisturbedHold disturbed_hold(GridSim& sim, SimDisturbance* noise, Direction direction, Milliseconds limit, SimPace pace){
    if (noise == nullptr){
        return {.move = sim.hold(direction, limit, pace)};
    }
    //  A tile at a time, at the hold's own pace.
    const Milliseconds tile_time = sim_ms_per_tile(direction, pace, sim.player().can_run);
    const int64_t tiles = limit < ROUTE_START_DELAY ? 0 : (limit - ROUTE_START_DELAY) / tile_time;
    DisturbedHold out{.move = {.stop = SimStop::LIMIT}};
    for (int64_t i = 0; i < tiles && out.move.stop == SimStop::LIMIT; i++){
        const SimMove step = sim.hold(direction, ROUTE_START_DELAY + tile_time, pace);
        out.move.distance += step.distance;
        out.move.stop = step.stop;
        if (step.stop == SimStop::LIMIT && noise->battle_at(sim.world(), nearest_tile(sim.player().pos))){
            out.battles++;
            out.first_tile |= i == 0;
        }
    }
    if (out.move.stop == SimStop::LIMIT){
        out.move.time = limit;
    }else{
        const Milliseconds seen = out.move.stop == SimStop::BLOCKED ? 800ms : 0ms;
        out.move.time = ROUTE_START_DELAY + tile_time * out.move.distance / SIM_TILE + seen;
    }
    if (out.move.stop == SimStop::LIMIT || out.move.stop == SimStop::BLOCKED){
        noise->walked(sim, direction, SIM_TILE, stopped_at_wall(sim, out.move, direction), noise->plan().rates.drift_per_tile);
    }
    return out;
}


WalkResult DisturbedRouteIO::walk_to_wall(const RouteWallWalk& walk){
    if (m_noise == nullptr){
        return GridSimRouteIO::walk_to_wall(walk);
    }
    const SimPace pace = walk.options.pace == DpadPace::RUN ? SimPace::RUN : SimPace::WALK;
    const DisturbedHold hold = disturbed_hold(m_sim, m_noise, walk.direction, walk.limit, pace);
    m_notes.push_back(std::format("[Sim] {}: held {} to {} at {}, {} wild battle(s).", walk.label, direction_name(walk.direction),
        detail::sim_stop_name(hold.move.stop), to_string(m_sim.player().pos), hold.battles));
    WalkResult result{.stop = walk_stop(hold.move.stop), .walked = hold.move.time, .wild_battles = hold.battles,
        .moved_chunks = (size_t)(hold.move.distance / SIM_TILE)};
    if (hold.first_tile){
        result.origin = WalkOrigin::MAY_PRECEDE_LEG;
    }
    return result;
}

//  A tap a tile, as GridSimRouteIO taps, drifting as the rates say; a battle on a tile stepped onto
//  is fled and credited as a moved tile, as the console's walk credits it,
//  and the taps go on. One on the first tap may have begun before it.
WalkResult DisturbedRouteIO::walk_taps(Direction direction, size_t tiles, const std::string& label){
    if (m_noise == nullptr){
        return GridSimRouteIO::walk_taps(direction, tiles, label);
    }
    WalkResult result{.stop = WalkStop::LIMIT, .walked = 0ms, .wild_battles = 0, .moved_chunks = 0};
    result.events.record({}, result.started);
    size_t still = 0;
    size_t moved_at_battle = 0;
    for (size_t tap = 0; tap < tiles && result.stop == WalkStop::LIMIT; tap++){
        const SimMove move = m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
        result.walked += 800ms;
        still = move.distance > 0 ? 0 : still + 1;
        result.moved_chunks += move.distance > 0 ? 1 : 0;
        if (move.stop == SimStop::DIALOG || move.stop == SimStop::WARP){
            result.stop = walk_stop(move.stop);
        }else if (still >= 2){
            result.stop = WalkStop::BLOCKED;
        }
        const int tap_drift = m_noise->plan().rates.tap_drift_per_tile;
        if (move.distance > 0 && result.stop == WalkStop::LIMIT && tap_drift > 0){
            m_noise->walked(m_sim, direction, SIM_TILE, false, tap_drift);
        }
        if (move.distance > 0 && move.stop == SimStop::DONE && m_noise->battle_at(m_sim.world(), nearest_tile(m_sim.player().pos))){
            result.wild_battles++;
            result.origin = tap == 0 ? WalkOrigin::MAY_PRECEDE_LEG : result.origin;
            //  The console's watchers time the battle from this tap.
            record_tap_battle(result, tap + 1);
            moved_at_battle = result.moved_chunks;
            m_notes.push_back("[Sim] wild battle during " + label);
        }
    }
    if (result.wild_battles > 0){
        result.moved_after_battle = result.moved_chunks - moved_at_battle;
    }
    m_notes.push_back(std::format("[Sim] {}: {} taps {}, {} moved, {} at {}.", label, tiles, direction_name(direction),
        result.moved_chunks, walk_stop_name(result.stop), to_string(m_sim.player().pos)));
    return result;
}

//  A stick press drifts with the tiles it moves.
void DisturbedRouteIO::press(const RouteInput& input){
    const SimPos before = m_sim.player().pos;
    GridSimRouteIO::press(input);
    if (m_noise == nullptr || input.kind != RouteInputKind::STICK){
        return;
    }
    const int moved = std::abs(along(m_sim.player().pos, input.direction) - along(before, input.direction));
    if (moved > 0){
        m_noise->walked(m_sim, input.direction, moved, false, m_noise->plan().rates.drift_per_tile);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
