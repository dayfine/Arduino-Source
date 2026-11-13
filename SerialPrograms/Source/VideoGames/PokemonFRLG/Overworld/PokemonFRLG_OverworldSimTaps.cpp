/*  Overworld Sim: taps and the Bicycle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walker's taps on the sim: turn taps, step taps, the steps of a
 *  hold, and on the Bicycle taps dropped when they come too fast, SELECT,
 *  and legs counted out a tap at a time.
 *
 */

#include <format>
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_OverworldFacing.h"
#include "PokemonFRLG_OverworldMaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace MovementMechanics;
using namespace OverworldWalkSteps;

namespace{

//  select_onto_bike()'s side on `sim`.
BikeSelectCalls sim_select_calls(OverworldSim& sim){
    BikeSelectCalls calls;
    calls.press_select = [&sim]{ sim.use_field(SimField::BIKE_SELECT); };
    calls.riding = [&sim]{ return sim.state().on_bike; };
    return calls;
}

} // namespace


//  Up to `left` steps of a hold of `direction` until `end`, the first
//  pressed at `pressed`.
void OverworldSim::take_steps(Direction direction, Milliseconds pressed, Milliseconds end, int left){
    for (; left > 0 && pressed < end; left--){
        //  Each step at the pace it starts at: on the water until a step
        //  lands the player, which takes a tile on foot (the hop).
        Milliseconds tile = tile_time();
        const std::optional<Step> step = press(direction);
        if (!step){
            bump(direction, pressed);
            return;
        }
        if (m_script.connections_stall && step->map != m_map && !step->warped){
            //  The step onto a connected map waits a tile while the game
            //  loads it: a hold timed by its tiles alone ended a tile short
            //  (console B, 2026-11-03, rows 04.06, 05.05 and 05.09).
            pressed += tile;
            if (pressed >= end){
                return;
            }
        }
        if (step->facing){
            m_state.facing = *step->facing;
        }
        pressed += after_slide(*step);
        if (after_step(*step, pressed)){
            return;
        }
        if (step->spin && m_script.holds_end_at_slide_stop){
            return;
        }
        tile = tile_time();
        pressed += step->ledge ? tile * 2 : tile;
    }
}

//  Whether a message shows after this step on the water, as one stopped a
//  hold on Route 21 (SimScript::messages_at_surf_steps).
bool OverworldSim::message_on_water(Milliseconds pressed){
    if (!m_script.messages_at_surf_steps.contains(++m_state.surf_steps)){
        return false;
    }
    m_frozen = true;
    schedule(pressed + m_script.timing.message, SimScreen::MESSAGE, false);
    note(std::format("a message on the water, surfed step {}.", m_state.surf_steps));
    return true;
}

bool OverworldSim::drops_bike_tap(Milliseconds start){
    const bool fast = m_last_bike_tap && start - *m_last_bike_tap < MovementMechanics::BIKE_TAP_DROP_PERIOD;
    m_last_bike_tap = start;
    if (!fast || ++m_fast_bike_taps % MovementMechanics::BIKE_FAST_TAP_DROP_EVERY != 0){
        return false;
    }
    m_state.bike_taps_dropped++;
    note(std::format("a Bicycle tap is dropped, {} fast taps in.", m_fast_bike_taps));
    return true;
}

bool OverworldSim::press_select(){
    const Milliseconds start = m_now;
    if (m_state.screen != SimScreen::OVERWORLD || m_frozen || !m_script.bike_registered){
        note(m_script.bike_registered ? "SELECT does nothing off the overworld." : "SELECT does nothing: the Bicycle isn't registered.");
        advance_to(start + MovementMechanics::BIKE_SELECT_PRESS);
        return false;
    }
    if (!m_state.on_bike && !map().allow_running){
        note("SELECT: the Bicycle can't be ridden here.");
        advance_to(start + MovementMechanics::BIKE_SELECT_TIME);
        return false;
    }
    m_state.on_bike = !m_state.on_bike;
    note(m_state.on_bike ? "gets on the Bicycle." : "gets off the Bicycle.");
    advance_to(start + MovementMechanics::BIKE_SELECT_TIME);
    return true;
}

bool OverworldSim::reads_step_tap_as_step(const MapPosition& before, int turn_taps_before){
    const bool moved = !(m_state.position == before);
    m_state.moved_step_taps += moved ? 1 : 0;
    const bool misread = moved ? m_script.unseen_step_taps.contains(m_state.moved_step_taps)
        : m_state.turn_taps > turn_taps_before && m_script.seen_turn_taps.contains(m_state.turn_taps);
    if (misread){
        note(moved ? std::format("step tap {} moves the player, read as not moved.", m_state.moved_step_taps) : std::format("turn tap {} only turns the player, read as a step.", m_state.turn_taps));
    }
    return moved != misread;
}

void OverworldSim::count_bike_leg(Direction direction, Milliseconds duration){
    const bool bike_tap = m_state.on_bike && duration <= MovementMechanics::LONGEST_TAP;
    if (bike_tap && m_bike_leg_direction != direction){
        m_state.bike_legs++;
        if (m_script.off_bike_at_legs.contains(m_state.bike_legs)){
            m_state.on_bike = false;
            note(std::format("bike leg {} starts off the Bicycle.", m_state.bike_legs));
        }
    }
    m_bike_leg_direction = bike_tap ? std::optional<Direction>(direction) : std::nullopt;
}

BikeTap OverworldSimWalker::bike_tap(const RouteStep& step){
    const MapPosition before = m_sim.state().position;
    const std::vector<Interrupt>& ints = interrupts(step);
    const Milliseconds release = m_rules.bike_tap_period - BIKE_TAP;
    const std::optional<SimSeen> seen = m_sim.hold(step.direction, BIKE_TAP, release, m_rules.bike_tap_period + BIKE_TAP_SETTLE, watchers(ints));
    if (seen){
        m_log.push_back("Bike: " + ints[seen->watcher].name + " on a tap of " + to_string(step) + ".");
        return BikeTap{.moved = false, .interrupt = seen->watcher};
    }
    if (!(m_sim.state().position == before)){
        return BikeTap{.moved = true};
    }
    return BikeTap{.moved = false, .interrupt = std::nullopt, .cleared_late = clear_late(ints, AFTER_LEG_WATCH, MAX_INTERRUPTIONS_PER_LEG) > 0};
}

StepOutcome OverworldSimWalker::walk_bike_taps(const RouteStep& step){
    const std::vector<Interrupt>& ints = interrupts(step);
    BikeTapCalls calls;
    calls.tap = [&](int){ return bike_tap(step); };
    calls.clear = [&](size_t index){ clear(ints[index]); };
    calls.log = [&](const std::string& message){ m_log.push_back(message); };
    const auto remount = [&]{
        if (m_sim.state().on_bike){
            return false;
        }
        m_log.push_back("Bike: a tap didn't move and the player is on foot: SELECT onto the Bicycle.");
        return select_onto_bike(sim_select_calls(m_sim));
    };
    m_leg_start = m_sim.state().time;
    m_leg_dark = m_sim.state().dark;
    return count_bike_taps_remounting(step, calls, remount);
}

BikeTap OverworldSimWalker::step_tap(const RouteStep& step, int tap){
    const MapPosition before = m_sim.state().position;
    const int turn_taps = m_sim.state().turn_taps;
    m_sim.tap(step.direction, STEP_TAP, STEP_TAP_RELEASE);
    m_sim.watch({}, step_tap_settle(step, tap));
    if (m_sim.reads_step_tap_as_step(before, turn_taps)){
        return BikeTap{.moved = true};
    }
    return BikeTap{.moved = false, .interrupt = std::nullopt, .cleared_late = clear_late(interrupts(step), AFTER_LEG_WATCH, MAX_INTERRUPTIONS_PER_LEG) > 0};
}

StepOutcome OverworldSimWalker::walk_step_taps(const RouteStep& step){
    const std::vector<Interrupt>& ints = interrupts(step);
    BikeTapCalls calls;
    calls.tap = [&](int tap){ return step_tap(step, tap); };
    calls.clear = [&](size_t index){ clear(ints[index]); };
    calls.log = [&](const std::string& message){ m_log.push_back(message); };
    calls.facing = [&]() -> std::optional<Direction>{
        if (m_sim.state().screen != SimScreen::OVERWORLD){
            return std::nullopt;
        }
        return m_sim.state().facing;
    };
    calls.tries = STEP_TAP_TRIES;
    m_leg_start = m_sim.state().time;
    m_leg_dark = m_sim.state().dark;
    return count_bike_taps(step, calls);
}

//  walk_and_watch(), getting on the Bicycle first where
//  walk_overworld_route_with_recovery() does.
StepOutcome OverworldSimWalker::walk(const RouteStep& step, std::optional<Direction> facing){
    if (m_rules.get_back_on_bike && gets_on_bike(step, m_rules.speed, m_riding)){
        if (!select_onto_bike(sim_select_calls(m_sim))){
            fail("get_on_bike(): not on the Bicycle after SELECT twice. Is it registered?");
        }
        m_riding = true;
        facing = std::nullopt;
    }
    m_speed = m_riding || !m_rules.get_back_on_bike ? m_rules.speed : on_foot(m_rules.speed);
    const StepOutcome outcome = walk_ridden(step, facing);
    if (m_riding && step.warps && !warp_keeps_bike(step.end.map)){
        m_log.push_back("Bike: through the warp to " + step.end.map + ", off the Bicycle.");
        m_riding = false;
    }
    return outcome;
}

bool sim_turn_to(OverworldSim& sim, Direction direction, bool confirm){
    if (!confirm){
        sim.tap(direction);
        return true;
    }
    const OverworldWalkSteps::TurnTaps turned = OverworldWalkSteps::tap_until_facing(
        direction, OverworldWalkSteps::TURN_FACING_READS,
        [&]{ sim.tap(direction); },
        [&](size_t read) -> std::optional<Direction>{
            //  Off the overworld (a message, a battle) the tap is taken as
            //  turned: what stopped it is the walker's to clear.
            if (sim.state().screen != SimScreen::OVERWORLD){
                return direction;
            }
            if (read <= sim.hidden_facing_reads()){
                return std::nullopt;
            }
            return sim.state().facing;
        }
    );
    //  The sim's walker knows the tile it turns on.
    return turned.facing || OverworldWalkSteps::unread_turn_taken({.at_known = true, .overworld_clear = true});
}

void sim_step_once(OverworldSim& sim, Direction direction){
    if (sim.state().facing != direction){
        sim_turn_to(sim, direction);
    }
    sim.tap(direction, STEP_TAP, STEP_TAP_RELEASE);
}


bool OverworldSimWalker::fade_over_since(Milliseconds from) const{
    const SimState& state = m_sim.state();
    if (state.screen == SimScreen::FADE){
        return false;
    }
    return std::any_of(
        state.shown.begin(), state.shown.end(),
        [&](const SimShown& shown){ return shown.screen == SimScreen::FADE && shown.at >= from; }
    );
}


//  A fade seen with no warp: the console's screen check
//  (warp_left_scene()) finds the scene from before the hold.
bool OverworldSimWalker::black_without_warp(const RouteStep& step, int warps_before, Milliseconds watched_from){
    const bool black_only = m_sim.state().warps == warps_before
        && (m_sim.state().screen == SimScreen::FADE || fade_over_since(watched_from));
    if (!black_only){
        return false;
    }
    if (m_rules.confirm_warp_destination){
        fail("walk_overworld_route(): the warp to " + step.end.map + " not confirmed: the screen after the fade shows the scene from before the hold.");
    }
    m_sim.watch({}, OverworldWalkSteps::AFTER_WARP);
    return true;
}

bool OverworldSim::black_without_warp(Milliseconds pressed){
    if (!m_script.black_without_warp.contains(++m_warp_steps)){
        return false;
    }
    m_frozen = true;
    schedule(pressed + m_script.timing.tile, SimScreen::FADE, false);
    schedule(pressed + m_script.timing.tile + m_script.timing.fade, SimScreen::OVERWORLD, true);
    note("black without a warp; the player stays at " + to_string(m_state.position) + ".");
    return true;
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
