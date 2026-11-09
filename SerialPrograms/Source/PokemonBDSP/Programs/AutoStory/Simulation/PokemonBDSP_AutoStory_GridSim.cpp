/*  BDSP AutoStory - Grid simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LegBattle.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WildBattlePolicy.h"
#include "PokemonBDSP_AutoStory_GridSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TileKind;
using Pokemon::TilePos;


namespace{

int floor_div(int value, int by){
    return value >= 0 ? value / by : -((-value + by - 1) / by);
}

bool across(Direction direction){
    return direction == Direction::EAST || direction == Direction::WEST;
}
//  +1 toward the east or south, -1 toward the west or north.
int sign_of(Direction direction){
    return direction == Direction::EAST || direction == Direction::SOUTH ? 1 : -1;
}
int& along(SimPos& pos, Direction direction){
    return across(direction) ? pos.x : pos.y;
}
int along(const SimPos& pos, Direction direction){
    return across(direction) ? pos.x : pos.y;
}
int sideways(const SimPos& pos, Direction direction){
    return across(direction) ? pos.y : pos.x;
}
TilePos tile_at(Direction direction, int along_tile, int side_tile){
    return across(direction) ? TilePos{along_tile, side_tile} : TilePos{side_tile, along_tile};
}

//  The tiles a player at `pos` moves into with their next tile toward
//  `direction`: one on the grid, two off it. `pos` is on the grid along
//  `direction`.
std::vector<TilePos> tiles_ahead(const SimPos& pos, Direction direction){
    const int next = along(pos, direction) / SIM_TILE + sign_of(direction);
    const int side = sideways(pos, direction);
    std::vector<TilePos> tiles{tile_at(direction, next, floor_div(side, SIM_TILE))};
    if (side % SIM_TILE != 0){
        tiles.push_back(tile_at(direction, next, floor_div(side, SIM_TILE) + 1));
    }
    return tiles;
}

bool is_door(const StoryWorld& world, const TilePos& pos){
    const std::optional<TileKind> tile = world.tile(pos);
    return tile && *tile == TileKind::DOOR && !world.occupied(pos);
}

//  Held time per tile of a walk east or west to a wall: slower than the
//  walker's 160 ms. On the console 3520 ms held east before the Running
//  Shoes went fewer than 20 tiles (lab-B 2026-11-09, 00.03): at least 176 ms
//  a tile. A limit timed at the walker's pace stops short here as it did
//  there. North and south keep their measured 240 ms.
constexpr Milliseconds SIM_WALK_MS_PER_TILE_ACROSS = Milliseconds(200);

Milliseconds per_tile(Direction direction, SimPace pace, bool can_run){
    if (pace == SimPace::RUN && can_run){
        return route_ms_per_tile(direction, MoveSpeed::RUN);
    }
    return std::max(route_ms_per_tile(direction, MoveSpeed::WALK), SIM_WALK_MS_PER_TILE_ACROSS);
}

uint32_t step_rng(uint64_t& state){
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<uint32_t>(state >> 32);
}

} // namespace


SimPos sim_pos(const TilePos& tile){
    return {tile.x * SIM_TILE, tile.y * SIM_TILE};
}
bool on_grid(const SimPos& pos){
    return pos.x % SIM_TILE == 0 && pos.y % SIM_TILE == 0;
}
TilePos nearest_tile(const SimPos& pos){
    return {floor_div(pos.x + SIM_TILE / 2, SIM_TILE), floor_div(pos.y + SIM_TILE / 2, SIM_TILE)};
}
std::string to_string(const SimPos& pos){
    return std::format("({:g}, {:g})", pos.x / (double)SIM_TILE, pos.y / (double)SIM_TILE);
}

const char* sim_stop_name(SimStop stop){
    switch (stop){
    case SimStop::DONE:
        return "done";
    case SimStop::BLOCKED:
        return "blocked";
    case SimStop::DIALOG:
        return "dialog";
    case SimStop::WARP:
        return "warp";
    case SimStop::LIMIT:
        return "limit";
    }
    return "?";
}


GridSim::GridSim(SimWorld world, const TilePos& start, Direction facing, bool can_run)
    : m_world(std::move(world))
    , m_wanderers(m_world.wanderers)
{
    m_player.pos = sim_pos(start);
    m_player.facing = facing;
    m_player.can_run = can_run;
    for (SimWanderer& wanderer : m_wanderers){
        if (wanderer.time_offset > 0ms){
            advance_wanderer(wanderer, wanderer.time_offset);
        }
    }
}

void GridSim::place(SimWorld world, const TilePos& start, std::optional<Direction> facing){
    m_world = std::move(world);
    m_wanderers = m_world.wanderers;
    m_player.pos = sim_pos(start);
    if (facing){
        m_player.facing = *facing;
    }
    m_player.dialog = false;
    m_player.trainer.reset();
    m_player.warped.reset();
    if (m_world.can_run){
        m_player.can_run = *m_world.can_run;
    }
    for (SimWanderer& wanderer : m_wanderers){
        if (wanderer.time_offset > 0ms){
            advance_wanderer(wanderer, wanderer.time_offset);
        }
    }
}

bool GridSim::can_wander_to(const SimWanderer& wanderer, const TilePos& next) const{
    if (next.x < wanderer.box.x0 || next.x > wanderer.box.x1 || next.y < wanderer.box.y0 || next.y > wanderer.box.y1){
        return false;
    }
    if (!standable(*m_world.world, next) || m_world.world->occupied(next) != StoryWorld::NOBODY){
        return false;
    }
    if (nearest_tile(m_player.pos) == next){
        return false;
    }
    for (const SimWanderer& other : m_wanderers){
        if (&other != &wanderer && other.pos == next){
            return false;
        }
    }
    return true;
}

void GridSim::step_wanderer(SimWanderer& wanderer){
    const uint32_t choice = step_rng(wanderer.seed) % 5;
    if (choice >= 4){
        return;
    }
    const Direction dir = static_cast<Direction>(choice);
    const TilePos next = wanderer.pos.next(dir);
    if (can_wander_to(wanderer, next)){
        wanderer.pos = next;
    }
}

void GridSim::advance_wanderer(SimWanderer& wanderer, Milliseconds time){
    wanderer.elapsed += time;
    while (wanderer.step_interval > 0ms && wanderer.elapsed >= wanderer.step_interval){
        wanderer.elapsed -= wanderer.step_interval;
        step_wanderer(wanderer);
    }
}

void sim_advance_time(GridSim& sim, Milliseconds time){
    for (SimWanderer& wanderer : sim.m_wanderers){
        sim.advance_wanderer(wanderer, time);
    }
}

const std::vector<SimWanderer>& sim_wanderers(const GridSim& sim){
    return sim.m_wanderers;
}

bool GridSim::wanderer_at(const TilePos& pos) const{
    for (const SimWanderer& wanderer : m_wanderers){
        if (wanderer.pos == pos){
            return true;
        }
    }
    return false;
}

//  Moves into the next tile toward `direction`, `part` of the way (a whole
//  tile or less), from a position on the grid along it; adds what it moved
//  to `moved`. A ledge hops two whole tiles and a door warps, however far
//  the press goes. Nothing if the player may go on, else why the move ended.
std::optional<SimStop> GridSim::enter_next(Direction direction, int part, int& moved){
    const StoryWorld& world = *m_world.world;
    const std::vector<TilePos> ahead = tiles_ahead(m_player.pos, direction);
    if (ahead.size() == 1){
        const TilePos from = nearest_tile(m_player.pos);
        const std::optional<TilePos> landing = PokemonBDSP::step(world, from, direction);
        if (landing && *landing != ahead[0]){
            part = 2 * SIM_TILE;
        }else if (!landing && (is_door(world, ahead[0]) || std::find(m_world.doors.begin(), m_world.doors.end(), ahead[0]) != m_world.doors.end())){
            m_player.pos = sim_pos(ahead[0]);
            moved += SIM_TILE;
            if (const std::optional<SimStop> stop = arrive()){
                return stop;
            }
            m_player.warped = ahead[0];
            return SimStop::WARP;
        }else if (!landing){
            return SimStop::BLOCKED;
        }
    }
    for (const TilePos& pos : ahead){
        if (wanderer_at(pos)){
            return SimStop::BLOCKED;
        }
    }
    //  Off the grid, both tiles must be open: a door frame or a corner stops us.
    for (const TilePos& pos : ahead){
        if (ahead.size() > 1 && !standable(world, pos)){
            return SimStop::BLOCKED;
        }
    }
    along(m_player.pos, direction) += sign_of(direction) * part;
    moved += part;
    return std::nullopt;
}

//  On reaching a whole tile: a scene's trigger or a trainer's sight stops us.
std::optional<SimStop> GridSim::arrive(){
    const TilePos tile = nearest_tile(m_player.pos);
    for (const SimTrigger& trigger : m_world.triggers){
        const bool fired = std::find(m_player.fired.begin(), m_player.fired.end(), trigger.name) != m_player.fired.end();
        if (!fired && std::find(trigger.tiles.begin(), trigger.tiles.end(), tile) != trigger.tiles.end()){
            m_player.fired.push_back(trigger.name);
            m_player.dialog = true;
            return SimStop::DIALOG;
        }
    }
    for (const TrainerSight& trainer : m_world.trainers){
        const bool fought = std::find(m_player.fought.begin(), m_player.fought.end(), trainer.name) != m_player.fought.end();
        const std::vector<TilePos> sight = sight_tiles(*m_world.world, trainer);
        if (!fought && std::find(sight.begin(), sight.end(), tile) != sight.end()){
            m_player.dialog = true;
            m_player.trainer = trainer.name;
            return SimStop::DIALOG;
        }
    }
    return std::nullopt;
}

//  Moves up to `distance` toward `direction`.
SimMove GridSim::advance(Direction direction, int distance){
    m_player.facing = direction;
    if (m_player.dialog){
        return {.stop = SimStop::DIALOG};
    }
    m_player.warped.reset();
    int moved = 0;
    while (moved < distance){
        const int offset = along(m_player.pos, direction) % SIM_TILE;
        if (offset != 0){
            //  Part of a tile to the next whole one: no new tile is entered.
            const int to_whole = sign_of(direction) > 0 ? SIM_TILE - offset : offset;
            const int part = std::min(to_whole, distance - moved);
            along(m_player.pos, direction) += sign_of(direction) * part;
            moved += part;
        }else if (const std::optional<SimStop> stop = enter_next(direction, std::min(SIM_TILE, distance - moved), moved)){
            return {.stop = *stop, .distance = moved};
        }
        if (along(m_player.pos, direction) % SIM_TILE == 0){
            if (const std::optional<SimStop> stop = arrive()){
                return {.stop = *stop, .distance = moved};
            }
        }
    }
    return {.stop = SimStop::DONE, .distance = moved};
}

SimMove GridSim::tap(Direction direction, Milliseconds hold, bool stick){
    const Milliseconds tile_time = route_ms_per_tile(direction, MoveSpeed::WALK);
    if (!stick && hold < 100ms){
        m_player.facing = direction;
        return {.stop = m_player.dialog ? SimStop::DIALOG : SimStop::DONE, .time = hold};
    }
    //  The stick moves as far as it is held; the D-pad whole tiles.
    int distance = (int)(hold.count() * SIM_TILE / tile_time.count());
    if (!stick){
        distance = std::max<int>(1, (int)((hold + tile_time / 2) / tile_time)) * SIM_TILE;
    }
    sim_advance_time(*this, hold);
    SimMove move = advance(direction, distance);
    move.time = hold;
    return move;
}

SimMove GridSim::hold(Direction direction, Milliseconds limit, SimPace pace){
    const Milliseconds tile_time = per_tile(direction, pace, m_player.can_run);
    const int64_t tiles = limit < ROUTE_START_DELAY ? 0 : (limit - ROUTE_START_DELAY) / tile_time;
    int moved = 0;
    SimStop stop = SimStop::LIMIT;
    while (moved < tiles * SIM_TILE){
        sim_advance_time(*this, tile_time);
        const SimMove step = advance(direction, SIM_TILE);
        moved += step.distance;
        if (step.stop != SimStop::DONE){
            stop = step.stop;
            break;
        }
    }
    if (stop == SimStop::LIMIT){
        return {.stop = stop, .distance = moved, .time = limit};
    }
    //  A stop is seen once the screen has held still a while.
    const Milliseconds seen = stop == SimStop::BLOCKED ? 800ms : 0ms;
    return {.stop = stop, .distance = moved, .time = ROUTE_START_DELAY + tile_time * moved / SIM_TILE + seen};
}

bool GridSim::press_a(){
    if (m_player.dialog){
        return true;
    }
    const StoryWorld& world = *m_world.world;
    const TilePos faced = nearest_tile(m_player.pos).next(m_player.facing);
    const std::optional<TileKind> tile = world.tile(faced);
    m_player.dialog = world.occupied(faced) != StoryWorld::NOBODY || (tile && *tile == TileKind::SIGN);
    for (const TrainerSight& trainer : m_world.trainers){
        if (m_player.dialog && trainer.pos == faced){
            m_player.trainer = trainer.name;
        }
    }
    for (const SimWanderer& wanderer : m_wanderers){
        if (wanderer.pos == faced){
            m_player.dialog = true;
        }
    }
    return m_player.dialog;
}

void GridSim::end_dialogue(){
    if (m_player.trainer){
        m_player.fought.push_back(*m_player.trainer);
    }
    m_player.trainer.reset();
    m_player.dialog = false;
}


bool sim_can_step(const GridSim& sim, Direction direction){
    GridSim probe = sim;
    const SimMove move = probe.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
    return move.distance > 0;
}


namespace{

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

//  The escape policy's game steps on a scripted battle.
class SimWildBattleEscape : public WildBattleEscapeActions{
public:
    SimWildBattleEscape(const SimWildBattle& battle, std::vector<std::string>& notes)
        : m_refusals_left(battle.run_refusals)
        , m_notes(notes)
    {}
    virtual WildRunOutcome run() override{
        if (m_refusals_left == 0){
            return WildRunOutcome::ESCAPED;
        }
        m_refusals_left--;
        return WildRunOutcome::TRAPPED;
    }
    virtual void fight() override{
        m_notes.push_back("[Sim escape] Fought with the best move until the battle ended.");
    }
    virtual void note(const std::string& text) override{
        m_notes.push_back("[Sim escape] " + text);
    }

private:
    size_t m_refusals_left;
    std::vector<std::string>& m_notes;
};

} // namespace

std::optional<SimWallEncounter> GridSimRouteIO::take_wall_encounter(const RouteWallWalk& walk){
    const auto found = std::find_if(m_wall_encounters.begin(), m_wall_encounters.end(), [&](const SimWallEncounter& encounter){
        return encounter.direction == walk.direction && encounter.label == walk.label;
    });
    if (found == m_wall_encounters.end()){
        return std::nullopt;
    }
    SimWallEncounter encounter = *found;
    m_wall_encounters.erase(found);
    return encounter;
}

void GridSimRouteIO::escape_scripted_battle(const SimWildBattle& battle, const std::string& label){
    SimWildBattleEscape actions(battle, m_notes);
    const WildEscapeResult result = escape_wild_battle(actions, WildBattleEscapePolicy());
    if (result == WildEscapeResult::UNCONFIRMED){
        note(std::format("escape_wild_battle: {}: the Run attempts ran out without the exit.", label), RouteNote::FAILURE);
    }
    const char* ended = result == WildEscapeResult::FOUGHT ? "fought to its end" : "run from";
    m_notes.push_back(std::format("[Sim] {}: wild battle {}.", label, ended));
}

WalkResult GridSimRouteIO::walk_to_wall(const RouteWallWalk& walk){
    size_t wild_battles = 0;
    if (const auto encounter = take_wall_encounter(walk)){
        escape_scripted_battle(encounter->battle, walk.label);
        wild_battles = 1;
    }
    const SimPace pace = walk.options.pace == DpadPace::RUN ? SimPace::RUN : SimPace::WALK;
    const SimMove move = m_sim.hold(walk.direction, walk.limit, pace);
    m_notes.push_back(std::format("[Sim] {}: held {} to {} at {}.", walk.label, direction_name(walk.direction),
        sim_stop_name(move.stop), to_string(m_sim.player().pos)));
    WalkResult result{.stop = walk_stop(move.stop), .walked = move.time, .wild_battles = wild_battles, .moved_chunks = (size_t)(move.distance / SIM_TILE)};
    result.warp.fade = move.stop == SimStop::WARP ? WarpFade::BLACK : WarpFade::NONE;
    return result;
}

std::optional<SimWildTap> GridSimRouteIO::take_encounter(Direction direction, size_t tiles, size_t tap, const std::string& label){
    const auto found = std::find_if(m_encounters.begin(), m_encounters.end(), [&](const SimWildTap& encounter){
        return encounter.direction == direction && encounter.requested_tiles == tiles
            && encounter.tap == tap && encounter.label == label;
    });
    if (found == m_encounters.end()){
        return std::nullopt;
    }
    SimWildTap encounter = *found;
    m_encounters.erase(found);
    return encounter;
}

//  A tap a tile, as walk_tiles_dpad() taps: two taps in a row that move
//  nothing end the walk.
WalkResult GridSimRouteIO::walk_taps(Direction direction, size_t tiles, const std::string& label){
    WalkResult result{.stop = WalkStop::LIMIT, .walked = 0ms, .wild_battles = 0, .moved_chunks = 0};
    result.events.record({}, result.started);
    size_t still = 0;
    size_t moved_at_battle = 0;
    for (size_t tap = 0; tap < tiles && result.stop == WalkStop::LIMIT; tap++){
        const auto encounter = take_encounter(direction, tiles, tap + 1, label);
        const SimMove move = encounter && encounter->phase == SimEncounterPhase::BEFORE_MOVEMENT
            ? SimMove{} : m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
        result.walked += 800ms;
        if (encounter){
            //  Match the production escape_battle() credit, independently
            //  of actual simulated displacement. Consumers must not infer
            //  pose solely from this credit after an interrupted input.
            result.wild_battles++;
            if (tap == 0 || encounter->origin == WalkOrigin::MAY_PRECEDE_LEG){
                result.origin = WalkOrigin::MAY_PRECEDE_LEG;
            }
            result.moved_chunks++;
            //  The console's watchers time the battle from the tap that set
            //  it off: this one, or for a battle before this tap moved, the
            //  one before (0: a step before the walk).
            record_tap_battle(result, encounter->phase == SimEncounterPhase::BEFORE_MOVEMENT ? tap : tap + 1);
            moved_at_battle = result.moved_chunks;
            still = 0;
            m_notes.push_back("[Sim] cleared wild encounter during " + label);
            continue;
        }
        still = move.distance > 0 ? 0 : still + 1;
        result.moved_chunks += move.distance > 0 ? 1 : 0;
        if (move.stop == SimStop::DIALOG || move.stop == SimStop::WARP){
            result.stop = walk_stop(move.stop);
            result.warp.fade = move.stop == SimStop::WARP ? WarpFade::BLACK : WarpFade::NONE;
        }else if (still >= 2){
            result.stop = WalkStop::BLOCKED;
        }
    }
    if (result.wild_battles > 0){
        result.moved_after_battle = result.moved_chunks - moved_at_battle;
    }
    m_notes.push_back(std::format("[Sim] {}: {} taps {}, {} moved, {} at {}.", label, tiles, direction_name(direction),
        result.moved_chunks, walk_stop_name(result.stop), to_string(m_sim.player().pos)));
    return result;
}

void GridSimRouteIO::press(const RouteInput& input){
    for (size_t press = 0; press < input.count; press++){
        switch (input.kind){
        case RouteInputKind::DPAD:
            m_sim.tap(input.direction, input.hold, false);
            break;
        case RouteInputKind::STICK:
            m_sim.tap(input.direction, input.hold, true);
            break;
        case RouteInputKind::BUTTON_A:
            m_sim.press_a();
            break;
        case RouteInputKind::WAIT:
        case RouteInputKind::PAUSE:
            sim_advance_time(m_sim, input.hold * input.count);
            break;
        }
    }
}

bool GridSimRouteIO::dialog_within(Milliseconds, Color){
    return m_sim.player().dialog;
}

bool GridSimRouteIO::holding_against_wall(Direction direction, const std::string&){
    return !sim_can_step(m_sim, direction);
}

void GridSimRouteIO::look(const std::function<void(const ImageViewRGB32& screen)>&){}

void GridSimRouteIO::note(const std::string& text, RouteNote kind){
    m_notes.push_back(text);
    if (kind == RouteNote::FAILURE){
        throw GridSimFailure{text};
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
