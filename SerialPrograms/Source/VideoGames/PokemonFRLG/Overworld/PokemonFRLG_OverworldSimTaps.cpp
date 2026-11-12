/*  Overworld Sim: taps and the Bicycle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walker's taps on the sim: turn taps, step taps, and on the Bicycle
 *  taps dropped when they come too fast, SELECT, and legs counted out a
 *  tap at a time.
 *
 */

#include <format>
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_OverworldBike.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace MovementMechanics;
using namespace OverworldWalkSteps;


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
    m_leg_start = m_sim.state().time;
    m_leg_dark = m_sim.state().dark;
    return count_bike_taps(step, calls);
}

BikeTap OverworldSimWalker::step_tap(const RouteStep& step, int tap){
    const MapPosition before = m_sim.state().position;
    m_sim.tap(step.direction, STEP_TAP, STEP_TAP_RELEASE);
    m_sim.watch({}, step_tap_settle(step, tap));
    if (!(m_sim.state().position == before) && !m_sim.hides_moved_step_tap()){
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
        BikeSelectCalls select;
        select.press_select = [&]{ m_sim.use_field(SimField::BIKE_SELECT); };
        select.riding = [&]{ return m_sim.state().on_bike; };
        if (!select_onto_bike(select)){
            fail("get_on_bike(): not on the Bicycle after SELECT twice. Is it registered?");
        }
        m_riding = true;
        facing = std::nullopt;
    }
    m_speed = m_riding || !m_rules.get_back_on_bike ? m_rules.speed : on_foot(m_rules.speed);
    const StepOutcome outcome = walk_ridden(step, facing);
    if (m_riding && step.warps){
        m_log.push_back("Bike: through the warp to " + step.end.map + ", off the Bicycle.");
        m_riding = false;
    }
    return outcome;
}

void sim_turn_to(OverworldSim& sim, Direction direction, bool confirm){
    for (size_t tap = 1; tap <= MAX_TURN_TAPS; tap++){
        sim.tap(direction);
        const SimState& state = sim.state();
        if (!confirm || state.screen != SimScreen::OVERWORLD || state.facing == direction){
            return;
        }
    }
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
