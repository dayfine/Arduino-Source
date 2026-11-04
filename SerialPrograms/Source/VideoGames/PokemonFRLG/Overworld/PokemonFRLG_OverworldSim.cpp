/*  Overworld Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_OverworldSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


const char* sim_screen_name(SimScreen screen){
    switch (screen){
    case SimScreen::OVERWORLD:
        return "the overworld";
    case SimScreen::MESSAGE:
        return "a message";
    case SimScreen::START_MENU:
        return "the START menu";
    case SimScreen::BATTLE_FLASH:
        return "a battle's flashes";
    case SimScreen::BATTLE_WIPE:
        return "a battle's wipe";
    case SimScreen::BATTLE_OPENING:
        return "a battle's opening";
    case SimScreen::BATTLE:
        return "a battle";
    case SimScreen::FADE:
        return "a warp's fade";
    }
    return "?";
}


namespace{

//  A short press: TURN_TAP and TURN_SETTLE.
constexpr Milliseconds TAP_TIME(340);
//  The story's battle loop takes a battle that doesn't show in this long
//  for none.
constexpr Milliseconds NO_BATTLE_WAIT(15000);
constexpr Milliseconds BATTLE_TIME(10000);
constexpr Milliseconds MESSAGE_CLOSE(1000);
//  The questions, "<MON> used CUT!" and the tree falling.
constexpr Milliseconds CUT_TIME(4000);
//  Rows of black below a small room, seen from this near its bottom.
constexpr int BLACK_BELOW_ROWS = 5;
constexpr int SMALL_ROOM_ROWS = 20;

int find_map(const std::vector<MapData>& maps, const std::string& name){
    for (size_t c = 0; c < maps.size(); c++){
        if (maps[c].name == name){
            return (int)c;
        }
    }
    return -1;
}

//  The way a ledge on `tile` hops the player, if it is one.
std::optional<Direction> ledge_direction(TileKind tile){
    switch (tile){
    case TileKind::LEDGE_NORTH:
        return Direction::NORTH;
    case TileKind::LEDGE_SOUTH:
        return Direction::SOUTH;
    case TileKind::LEDGE_WEST:
        return Direction::WEST;
    case TileKind::LEDGE_EAST:
        return Direction::EAST;
    default:
        return std::nullopt;
    }
}

//  The tile next to `player` a trainer standing on `box` walks up to.
TilePos trainer_stop(const Pokemon::TileBox& box, const TilePos& player){
    if (box.x0 <= player.x && player.x <= box.x1){
        return TilePos{player.x, box.y0 > player.y ? player.y + 1 : player.y - 1};
    }
    return TilePos{box.x0 > player.x ? player.x + 1 : player.x - 1, player.y};
}

}


OverworldSim::OverworldSim(std::vector<MapData> maps, MapPosition start, Direction facing, SimScript script)
    : m_maps(std::move(maps))
    , m_script(std::move(script))
    , m_map(find_map(m_maps, start.map))
{
    if (m_map < 0){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Sim: start on a map that isn't loaded: " + start.map);
    }
    m_player = PlayerState{start.point, elevation_at(map(), start.point), false};
    m_state.facing = facing;
    m_state.screen = m_script.start_menu_open ? SimScreen::START_MENU : SimScreen::OVERWORLD;
    update_view();
}


void OverworldSim::note(const std::string& line){
    m_state.log.push_back(std::format("[Sim {} ms] {}", m_now.count(), line));
}
void OverworldSim::violation(const std::string& line){
    note("violation: " + line);
    m_state.violations.push_back(line);
}

void OverworldSim::update_view(){
    m_state.position = MapPosition{map().name, m_player.position};
    m_state.black_below = map().connections.empty() && map().tiles.height <= SMALL_ROOM_ROWS
        && m_player.position.y >= map().tiles.height - BLACK_BELOW_ROWS;
}

void OverworldSim::schedule(Milliseconds at, SimScreen screen, bool unfreeze){
    const Pending pending{at, screen, unfreeze};
    const auto iter = std::upper_bound(
        m_pending.begin(), m_pending.end(), pending,
        [](const Pending& a, const Pending& b){ return a.at < b.at; }
    );
    m_pending.insert(iter, pending);
}

void OverworldSim::advance_to(Milliseconds time){
    while (!m_pending.empty() && m_pending.front().at <= time){
        m_state.screen = m_pending.front().screen;
        m_frozen = m_frozen && !m_pending.front().unfreeze;
        m_pending.erase(m_pending.begin());
    }
    m_now = std::max(m_now, time);
}


bool OverworldSim::sees(SimWatcher watcher, SimScreen screen) const{
    switch (watcher){
    case SimWatcher::FLASH:
        return screen == SimScreen::BATTLE_FLASH || screen == SimScreen::FADE;
    case SimWatcher::BATTLE_SCREEN:
        return screen == SimScreen::BATTLE;
    case SimWatcher::BATTLE_TEXT_BOX:
        return screen == SimScreen::BATTLE || screen == SimScreen::BATTLE_OPENING || (screen == SimScreen::OVERWORLD && m_state.black_below);
    case SimWatcher::MESSAGE:
        return screen == SimScreen::MESSAGE;
    case SimWatcher::FADE:
        return screen == SimScreen::FADE || screen == SimScreen::BATTLE_WIPE;
    }
    return false;
}

std::optional<Milliseconds> OverworldSim::first_seen(SimWatcher watcher, Milliseconds from, Milliseconds to) const{
    if (sees(watcher, m_state.screen)){
        return from;
    }
    for (const Pending& pending : m_pending){
        if (pending.at > to){
            break;
        }
        if (pending.at > from && sees(watcher, pending.screen)){
            return pending.at;
        }
    }
    return std::nullopt;
}

std::optional<SimSeen> OverworldSim::watch_until(const std::vector<SimWatcher>& watchers, Milliseconds from, Milliseconds to){
    advance_to(from);
    std::optional<SimSeen> seen;
    std::optional<Milliseconds> when;
    for (size_t c = 0; c < watchers.size(); c++){
        const std::optional<Milliseconds> at = first_seen(watchers[c], from, to);
        if (at && (!when || *at < *when)){
            when = at;
            seen = SimSeen{c, *at - from};
        }
    }
    advance_to(when.value_or(to));
    return seen;
}


std::optional<OverworldSim::Step> OverworldSim::take_warp(int map_index, const TilePos& tile) const{
    const MapWarp* warp = warp_at(m_maps[map_index], tile);
    if (warp == nullptr || !warp->to){
        return std::nullopt;
    }
    const int destination = find_map(m_maps, warp->to->map);
    if (destination < 0){
        return std::nullopt;
    }
    const MapData& to = m_maps[destination];
    const auto arrival = to.points.find(warp->to->point);
    if (arrival == to.points.end()){
        return std::nullopt;
    }
    TilePos position = arrival->second;
    const bool walked_out = walks_out_on_arrival(to, position);
    if (walked_out){
        position = position.next(Direction::SOUTH);
    }
    return Step{destination, PlayerState{position, elevation_at(to, position), false}, true, walked_out, false};
}

std::optional<OverworldSim::Step> OverworldSim::cross_edge(Direction direction) const{
    for (const MapConnection& connection : map().connections){
        const int destination = connection.side == direction ? find_map(m_maps, connection.to_map) : -1;
        if (destination < 0){
            continue;
        }
        const MapData& to = m_maps[destination];
        const TilePos here = m_player.position;
        TilePos point{here.x - connection.offset, here.y - connection.offset};
        switch (direction){
        case Direction::NORTH: point.y = to.tiles.height - 1; break;
        case Direction::SOUTH: point.y = 0; break;
        case Direction::WEST: point.x = to.tiles.width - 1; break;
        case Direction::EAST: point.x = 0; break;
        }
        const StepKind kind = classify_step_onto(map(), m_player, direction, to, point);
        if (kind != StepKind::WALK){
            return std::nullopt;
        }
        return Step{destination, after_step_onto(map(), m_player, to, point, kind), false, false, false};
    }
    return std::nullopt;
}

//  Pressing `direction` once, as the planner's press() reads the game, and
//  also hopping a ledge the way it faces.
std::optional<OverworldSim::Step> OverworldSim::press(Direction direction) const{
    const TilePos here = m_player.position;
    if (exit_mat(map(), here) == direction){
        return take_warp(m_map, here);
    }
    const TilePos ahead = here.next(direction);
    if (direction == Direction::NORTH && is_door(map(), ahead)){
        return take_warp(m_map, ahead);
    }
    if (!in_bounds(map(), ahead)){
        return cross_edge(direction);
    }
    if (ledge_direction(tile_at(map(), ahead)) == direction){
        const TilePos landed = ahead.next(direction);
        return Step{m_map, PlayerState{landed, elevation_at(map(), landed), false}, false, false, true};
    }
    const StepKind kind = classify_step(map(), m_player, direction);
    if (kind != StepKind::WALK){
        return std::nullopt;
    }
    const Step step{m_map, after_step_onto(map(), m_player, map(), ahead, kind), false, false, false};
    if (warps_on_step(map(), ahead)){
        return take_warp(m_map, ahead);
    }
    return step;
}


//  A trainer on the player's map who sees them now walks up and speaks.
bool OverworldSim::trainer_sees(Milliseconds pressed){
    std::vector<MapObject>& objects = m_maps[m_map].objects;
    for (size_t c = 0; c < objects.size(); c++){
        MapObject& trainer = objects[c];
        if (!trainer.trainer || !trainer_may_see(map(), trainer, m_player.position)){
            continue;
        }
        const Pokemon::TileBox box = trainer.wander.value_or(Pokemon::TileBox{trainer.x, trainer.y, trainer.x, trainer.y});
        const TilePos stop = trainer_stop(box, m_player.position);
        const int approach = std::abs(stop.x - trainer.x) + std::abs(stop.y - trainer.y);
        trainer.x = stop.x;
        trainer.y = stop.y;
        trainer.wander.reset();
        //  The player turns to face the trainer.
        for (Direction direction : {Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST}){
            m_state.facing = m_player.position.next(direction) == stop ? direction : m_state.facing;
        }
        m_trainer_speaking = c;
        m_frozen = true;
        schedule(pressed + m_script.timing.tile * (1 + approach) + m_script.timing.trainer_notice, SimScreen::MESSAGE, false);
        note(std::format("{} on {} sees the player, walks up {} tile(s) to {}.", trainer.name, map().name, approach, to_string(stop)));
        return true;
    }
    return false;
}

//  The player has taken `step`, pressed at `pressed`. Returns whether the
//  game stopped them there: a warp, a wild battle, a trainer.
bool OverworldSim::after_step(const Step& step, Milliseconds pressed){
    const SimTiming& timing = m_script.timing;
    m_map = step.map;
    m_player = step.player;
    m_state.steps++;
    update_view();
    if (step.warped){
        m_state.warps++;
        for (auto& [index, tree] : m_cut_trees){
            m_maps[index].objects.push_back(tree);
        }
        m_cut_trees.clear();
        m_frozen = true;
        schedule(pressed + timing.tile, SimScreen::FADE, false);
        schedule(pressed + timing.tile + timing.fade, SimScreen::OVERWORLD, true);
        note("warped to " + to_string(m_state.position) + ".");
        return true;
    }
    return wild_battle(pressed) || trainer_sees(pressed);
}

//  A wild battle the script starts on this step onto tall grass, or onto a
//  wild floor.
bool OverworldSim::wild_battle(Milliseconds pressed){
    if (!is_tall_grass(map(), m_player.position) && !m_script.wild_floor_maps.contains(map().name)){
        return false;
    }
    m_grass_steps++;
    for (const SimEncounter& encounter : m_script.encounters){
        if (encounter.grass_step != m_grass_steps){
            continue;
        }
        m_frozen = true;
        m_battle_pending = true;
        if (!encounter.seen_late){
            schedule(pressed + m_script.timing.flash, SimScreen::BATTLE_FLASH, false);
        }
        if (m_script.timing.battle_wipe){
            schedule(pressed + *m_script.timing.battle_wipe, SimScreen::BATTLE_WIPE, false);
        }
        if (m_script.timing.battle_opening){
            schedule(pressed + *m_script.timing.battle_opening, SimScreen::BATTLE_OPENING, false);
        }
        schedule(pressed + m_script.timing.battle_screen, SimScreen::BATTLE, false);
        note(std::format("a wild battle on {}{}.", to_string(m_state.position), encounter.seen_late ? " (flashes missed)" : ""));
        return true;
    }
    return false;
}

//  Blocked holding `direction` at `pressed`: a sign walked into moving north
//  or south shows its message.
void OverworldSim::bump(Direction direction, Milliseconds pressed){
    const TilePos ahead = m_player.position.next(direction);
    const bool sign = tile_at(map(), ahead) == TileKind::SIGN
        && (direction == Direction::NORTH || direction == Direction::SOUTH);
    if (!sign){
        return;
    }
    m_state.signs_read++;
    m_frozen = true;
    schedule(pressed + m_script.timing.message, SimScreen::MESSAGE, false);
    note("reads the sign on " + to_string(ahead) + ".");
}

//  The steps a hold of `duration` takes, `short_by` fewer.
void OverworldSim::walk_hold(Direction direction, Milliseconds duration, int short_by){
    const Milliseconds tile = m_script.timing.tile;
    const Milliseconds end = m_now + duration;
    Milliseconds pressed = m_now;
    if (m_state.facing != direction){
        //  Holding through a turn lost a tile on the console.
        m_state.facing = direction;
        pressed += tile;
    }
    int left = (int)((end - pressed + tile - 1ms) / tile) - short_by;
    for (; left > 0 && pressed < end; left--){
        const std::optional<Step> step = press(direction);
        if (!step){
            bump(direction, pressed);
            return;
        }
        if (m_script.connections_stall && step->map != m_map && !step->warped){
            //  The step onto a connected map waits a tile while the game
            //  loads it: a hold timed by its tiles alone ended a tile short
            //  (console B, 2026-11-03, rows 00.14, 00.16 and 00.20).
            pressed += tile;
            if (pressed >= end){
                return;
            }
        }
        if (step->walked_out){
            m_state.facing = Direction::SOUTH;
        }
        if (after_step(*step, pressed)){
            return;
        }
        pressed += step->ledge ? tile * 2 : tile;
    }
}

//  A hold the game doesn't move the player for.
void OverworldSim::stuck_hold(Direction direction){
    if (m_state.screen == SimScreen::START_MENU){
        note(std::string("the D-pad moves the START menu's cursor (") + direction_name(direction) + ").");
    }else if (m_battle_pending){
        violation(std::string("the D-pad held ") + direction_name(direction) + " into a battle.");
    }else{
        note(std::string("the D-pad held ") + direction_name(direction) + " under " + sim_screen_name(m_state.screen) + ".");
    }
}


std::optional<SimSeen> OverworldSim::hold(
    Direction direction, Milliseconds duration, Milliseconds release, Milliseconds watch,
    const std::vector<SimWatcher>& watchers
){
    const Milliseconds start = m_now;
    m_holds++;
    if (m_state.screen != SimScreen::OVERWORLD || m_frozen){
        stuck_hold(direction);
    }else{
        const auto short_hold = m_script.short_holds.find(m_holds);
        walk_hold(direction, duration, short_hold == m_script.short_holds.end() ? 0 : short_hold->second);
    }
    return watch_until(watchers, start, start + std::max(duration + release, watch));
}

void OverworldSim::tap(Direction direction){
    if (m_state.screen != SimScreen::OVERWORLD || m_frozen){
        stuck_hold(direction);
    }else if (m_state.facing != direction){
        m_state.facing = direction;
    }else{
        walk_hold(direction, m_script.timing.tile / 2, 0);
    }
    advance_to(m_now + TAP_TIME);
}

std::optional<SimSeen> OverworldSim::watch(const std::vector<SimWatcher>& watchers, Milliseconds duration){
    return watch_until(watchers, m_now, m_now + duration);
}

SimBattleEnd OverworldSim::handle_battle(SimBattleChoice choice){
    if (!m_battle_pending){
        violation("fought a battle that isn't there, on " + std::string(sim_screen_name(m_state.screen)) + ".");
        advance_to(m_now + NO_BATTLE_WAIT);
        return SimBattleEnd::NO_BATTLE;
    }
    while (m_state.screen != SimScreen::BATTLE && !m_pending.empty()){
        advance_to(m_pending.front().at);
    }
    if (m_trainer && choice == SimBattleChoice::RUN){
        violation("ran from a trainer's battle.");
        advance_to(m_now + BATTLE_TIME);
        return SimBattleEnd::CANT_RUN;
    }
    advance_to(m_now + BATTLE_TIME);
    m_state.screen = SimScreen::OVERWORLD;
    m_frozen = false;
    m_battle_pending = false;
    if (m_trainer){
        MapObject& trainer = m_maps[m_map].objects[*m_trainer];
        trainer.trainer.reset();
        m_state.trainers_beaten.push_back(trainer.name);
        m_trainer.reset();
    }
    note(std::string(choice == SimBattleChoice::RUN ? "ran from" : "won") + " the battle.");
    return SimBattleEnd::ENDED;
}

bool OverworldSim::close_message(){
    if (m_state.screen != SimScreen::MESSAGE){
        return false;
    }
    advance_to(m_now + MESSAGE_CLOSE);
    m_state.screen = SimScreen::OVERWORLD;
    if (!m_trainer_speaking){
        m_frozen = false;
        return false;
    }
    m_trainer = m_trainer_speaking;
    m_trainer_speaking.reset();
    m_battle_pending = true;
    schedule(m_now + m_script.timing.trainer_battle, SimScreen::BATTLE, false);
    note("the trainer's battle starts.");
    return true;
}

bool OverworldSim::cut_tree(){
    std::vector<MapObject>& objects = m_maps[m_map].objects;
    const TilePos ahead = m_player.position.next(m_state.facing);
    for (size_t c = 0; c < objects.size(); c++){
        if (objects[c].name == "cut_tree" && objects[c].x == ahead.x && objects[c].y == ahead.y
            && m_state.screen == SimScreen::OVERWORLD && !m_frozen
        ){
            m_cut_trees.emplace_back(m_map, objects[c]);
            objects.erase(objects.begin() + c);
            advance_to(m_now + CUT_TIME);
            note("cut the tree on " + to_string(MapPosition{map().name, ahead}) + ".");
            return true;
        }
    }
    violation("no tree to cut " + std::string(direction_name(m_state.facing)) + " of " + to_string(m_state.position) + ".");
    return false;
}



//
//  OverworldSimWalker
//

namespace{

using namespace OverworldWalkSteps;

//  As the console's watch for a trainer (PokemonFRLG_OverworldRecoveryWalk.cpp).
constexpr Milliseconds TRAINER_NOTICE(1500);
constexpr Milliseconds TRAINER_SPEAKS(1000);
//  The console's wait for a warp's fade (wait_out_warp()).
constexpr Milliseconds WARP_TIMEOUT(8000);

//  The detection delay of the walker's interrupt `index` in `interrupts`.
Milliseconds delay_of(const OverworldInterrupts& interrupts, size_t index){
    return interrupts[index]->detection_delay();
}

}


OverworldSimWalker::OverworldSimWalker(OverworldSim& sim, SimWalkerRules rules, const OverworldInterruptSet& walker)
    : m_sim(sim)
    , m_rules(rules)
{
    //  In the walker's order: battles first.
    m_wild = {
        {"a wild battle", SimWatcher::FLASH, delay_of(walker.wild, 0), true},
        {"a wild battle (seen late)", SimWatcher::BATTLE_SCREEN, delay_of(walker.wild, 1), true},
        {"a message", SimWatcher::MESSAGE, delay_of(walker.wild, 2), false},
    };
    m_other = {
        {"a message", SimWatcher::MESSAGE, delay_of(walker.other, 0), false},
    };
}

void OverworldSimWalker::fail(const std::string& message){
    m_log.push_back("FAILED: " + message);
    throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, message);
}

const std::vector<OverworldSimWalker::Interrupt>& OverworldSimWalker::interrupts(const RouteStep& step) const{
    return step.wild ? m_wild : m_other;
}

std::vector<SimWatcher> OverworldSimWalker::watchers(const std::vector<Interrupt>& interrupts) const{
    std::vector<SimWatcher> ret;
    for (const Interrupt& interrupt : interrupts){
        const bool old_battle_screen = interrupt.watcher == SimWatcher::BATTLE_SCREEN && !m_rules.battle_needs_hp_box;
        ret.push_back(old_battle_screen ? SimWatcher::BATTLE_TEXT_BOX : interrupt.watcher);
    }
    return ret;
}

//  The story's interrupts: a battle is run from (a trainer's can't be), a
//  message closed and a trainer's battle after it fought.
void OverworldSimWalker::clear(const Interrupt& interrupt){
    if (interrupt.battle){
        if (m_sim.handle_battle(SimBattleChoice::RUN) == SimBattleEnd::CANT_RUN){
            fail("Failed to flee the battle: it is a trainer's.");
        }
        return;
    }
    if (m_sim.close_message() && m_rules.fight_trainer_after_message){
        m_sim.handle_battle(SimBattleChoice::FIGHT);
    }
}

//  Pokemon::clear_late_interrupts().
size_t OverworldSimWalker::clear_late(const std::vector<Interrupt>& interrupts, Milliseconds watch, size_t max){
    for (size_t count = 0; ; count++){
        const std::optional<SimSeen> seen = m_sim.watch(watchers(interrupts), watch);
        if (!seen){
            return count;
        }
        if (count >= max){
            fail(std::format("Interrupted {} times in a row after a leg.", count));
        }
        m_log.push_back("Leg: " + interrupts[seen->watcher].name + " at the end of the leg.");
        clear(interrupts[seen->watcher]);
    }
}

void OverworldSimWalker::check_overworld(const RouteStep& step){
    const SimScreen screen = m_sim.state().screen;
    if (m_rules.check_overworld && (screen == SimScreen::START_MENU || screen == SimScreen::MESSAGE)){
        fail(std::string("walk_overworld_route(): ") + sim_screen_name(screen) + " is open, before: " + to_string(step));
    }
}

std::optional<SimSeen> OverworldSimWalker::hold_watching(const RouteStep& step, Milliseconds hold, const std::vector<Interrupt>& interrupts){
    return m_sim.hold(step.direction, hold, STEP_RELEASE, hold + STEP_RELEASE + m_rules.watch_past_hold, watchers(interrupts));
}

//  wait_out_warp(), then run_from_battle_after_warp(): false if the fade
//  was a wild battle's, run from. wait_out_warp()'s watcher takes a wild
//  battle's wipe for the fade (run 7; console B, 2026-11-03, Mt. Moon 1F):
//  the battle screen stands in for the wipe.
bool OverworldSimWalker::after_warp(const RouteStep& step, int warps_before){
    if (m_sim.state().warps == warps_before){
        if (!m_sim.watch({SimWatcher::BATTLE_SCREEN}, WARP_TIMEOUT)){
            fail("walk_overworld_route(): no fade in and out after walking onto the warp to " + step.end.map + ".");
        }
    }else{
        m_sim.watch({SimWatcher::FADE}, WARP_TIMEOUT);
    }
    m_sim.watch({}, AFTER_WARP);
    if (!m_sim.watch(watchers({m_wild[1]}), AFTER_WARP_BATTLE_WATCH - AFTER_WARP)){
        return true;
    }
    m_sim.handle_battle(SimBattleChoice::RUN);
    return false;
}

void OverworldSimWalker::fail_off_route(const RouteStep& step){
    fail("walk_overworld_route(): a wild battle, not the warp to " + step.end.map + ": the player is off the route.");
}

std::optional<WalkedTiles> OverworldSimWalker::walk_step(const RouteStep& step, std::optional<Direction> facing){
    if (step.action == RouteAction::FACE){
        m_sim.tap(step.direction);
        return std::nullopt;
    }
    if (step.action != RouteAction::WALK){
        fail("Sim: Surf isn't modelled.");
    }
    if (facing != step.direction){
        m_sim.tap(step.direction);
    }
    const int warps_before = m_sim.state().warps;
    const std::vector<Interrupt>& ints = interrupts(step);
    Pokemon::LegSpec leg;
    leg.direction = step.direction;
    leg.tiles = step.tiles + step.warp_slack;
    leg.to_wall = step.to_wall;
    leg.tile_time = TILE_TIME;
    leg.stop_on_interrupt = !step.to_wall;
    leg.stop_if_blocked = step.to_wall && m_rules.stop_if_blocked;
    if (m_rules.hold_over_connections){
        leg.stalled_steps = step.connection_steps;
    }
    Pokemon::LegCalls calls;
    calls.hold_watching = [&](Milliseconds hold) -> std::optional<Pokemon::HoldOutcome>{
        const std::optional<SimSeen> seen = hold_watching(step, hold, ints);
        return seen ? std::optional<Pokemon::HoldOutcome>(Pokemon::HoldOutcome{seen->watcher, seen->elapsed}) : std::nullopt;
    };
    calls.clear = [&](size_t index){ clear(ints[index]); };
    calls.detection_delay = [&](size_t index){ return ints[index].delay; };
    calls.name = [&](size_t index){ return ints[index].name; };
    calls.log = [&](const std::string& message){ m_log.push_back("Leg: " + message); };
    calls.fail = [&](const std::string& message){ fail(message); };
    const Pokemon::LegResult result = Pokemon::walk_leg_with(leg, calls, MAX_INTERRUPTIONS_PER_LEG);
    if (result.stopped){
        return walked_near(step, result.tiles, TILE_TIME);
    }
    if (step.warps && !after_warp(step, warps_before)){
        if (!m_rules.find_player_after_warp_battle){
            fail_off_route(step);
        }
        m_log.push_back("Route: a wild battle, not the warp to " + step.end.map + "; the player is on " + to_string(step) + " before the warp.");
        return walked_before_warp(step);
    }
    return std::nullopt;
}

//  walk_and_watch().
StepOutcome OverworldSimWalker::walk(const RouteStep& step, std::optional<Direction> facing){
    if (m_rules.battle_before_step && battle_on_its_way()){
        m_log.push_back("Route: " + std::string(sim_screen_name(m_sim.state().screen)) + " before " + to_string(step) + ": watching on for a wild battle set off before it.");
        if (clear_late(m_wild, UNSEEN_BATTLE_WATCH, MAX_INTERRUPTIONS_PER_LEG) > 0){
            return StepOutcome{.end = StepEnd::BATTLE_BEFORE, .walked = std::nullopt};
        }
    }
    if (m_sim.state().screen == SimScreen::MESSAGE){
        m_log.push_back("Route: a message is open before " + to_string(step) + "; handing it to the leg's interrupts.");
        clear_late(interrupts(step), OPEN_MESSAGE_WATCH, 1);
    }
    check_overworld(step);
    if (const std::optional<WalkedTiles> walked = walk_step(step, facing)){
        return StepOutcome{.end = StepEnd::STOPPED, .walked = walked};
    }
    if (step.action == RouteAction::FACE){
        return StepOutcome{};
    }
    size_t late = clear_late(interrupts(step), step.wild ? m_rules.after_wild_leg_watch : AFTER_LEG_WATCH, MAX_INTERRUPTIONS_PER_LEG);
    if (late == 0){
        late = clear_unseen_battle(step);
    }
    const bool done = late == 0 || step.warps || step.action != RouteAction::WALK;
    return StepOutcome{.end = done ? StepEnd::DONE : StepEnd::LATE, .walked = std::nullopt};
}

//  Whether the screen shows a battle on its way: its wipe's black, or its
//  text box before the HP boxes.
bool OverworldSimWalker::battle_on_its_way() const{
    const SimScreen screen = m_sim.state().screen;
    return screen == SimScreen::BATTLE_WIPE || screen == SimScreen::BATTLE_OPENING;
}

//  clear_unseen_battle(): the battle's text box or wipe seen after the
//  leg's watch.
size_t OverworldSimWalker::clear_unseen_battle(const RouteStep& step){
    const bool checked = step.wild ? m_rules.watch_on_for_unseen_battle : m_rules.watch_on_after_unmarked_leg;
    if (!checked || step.action != RouteAction::WALK || step.warps || !battle_on_its_way()){
        return 0;
    }
    m_log.push_back("Route: " + std::string(sim_screen_name(m_sim.state().screen)) + " after " + to_string(step) + ", which nothing stopped: watching on for a wild battle.");
    return clear_late(m_wild, UNSEEN_BATTLE_WATCH, MAX_INTERRUPTIONS_PER_LEG);
}

void OverworldSimWalker::hold_to_warp(const RouteStep& step){
    std::vector<SimWatcher> fade_first{SimWatcher::FADE};
    for (const SimWatcher watcher : watchers(m_other)){
        fade_first.push_back(watcher);
    }
    const int warps_before = m_sim.state().warps;
    for (size_t count = 0; count <= MAX_INTERRUPTIONS_PER_LEG; count++){
        m_sim.tap(step.direction);
        const Milliseconds hold = TILE_TIME * (step.tiles + 2);
        const std::optional<SimSeen> seen = m_sim.hold(step.direction, hold, STEP_RELEASE, hold + STEP_RELEASE, fade_first);
        if (seen && seen->watcher == 0){
            if (!after_warp(step, warps_before)){
                fail_off_route(step);
            }
            return;
        }
        if (!seen){
            break;
        }
        clear(m_other[seen->watcher - 1]);
    }
    fail("walk_overworld_route(): no fade holding on toward the warp to " + step.end.map + ".");
}

StepEnd OverworldSimWalker::watch_for_trainer(const RouteStep& step, int approach_tiles){
    const Milliseconds watch = TRAINER_NOTICE + TILE_TIME * approach_tiles + TRAINER_SPEAKS;
    return clear_late(interrupts(step), watch, MAX_INTERRUPTIONS_PER_LEG) == 0 ? StepEnd::DONE : StepEnd::LATE;
}

bool OverworldSimWalker::moved_past_wall(const RouteStep& step){
    const MapPosition before = m_sim.state().position;
    m_sim.tap(step.direction);
    return !(m_sim.state().position == before);
}

RouteCalls OverworldSimWalker::calls(){
    RouteCalls calls;
    calls.walk = [this](const RouteStep& step, std::optional<Direction> facing){ return walk(step, facing); };
    calls.hold_to_warp = [this](const RouteStep& step){ hold_to_warp(step); };
    calls.log = [this](const std::string& message){ m_log.push_back(message); };
    calls.fail = [this](const std::string& message){ fail(message); };
    calls.moved_past_wall = [this](const RouteStep& step){ return moved_past_wall(step); };
    calls.watch_for_trainer = [this](const RouteStep& step, int approach_tiles){ return watch_for_trainer(step, approach_tiles); };
    return calls;
}

std::string OverworldSimWalker::run(const std::vector<RouteStep>& route, const RoutePlanning& planning){
    try{
        run_route(route, m_sim.state().facing, &planning, calls());
    }catch (OperationFailedException& e){
        return e.message();
    }
    return "";
}



}
}
}
