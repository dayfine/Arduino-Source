/*  Screen Machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <string>
#include <utility>
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "ScreenMachine.h"

namespace PokemonAutomation{


const ScreenState* match_screen_state(const std::vector<ScreenState>& states, const ScreenObservation& observation){
    for (const ScreenState& state : states){
        if (state.matches && state.matches(observation)){
            return &state;
        }
    }
    return nullptr;
}


namespace{


//  The checks between looks: no state may come up too often, and steps must
//  keep changing the screen (a waiting state's only for its
//  ScreenState::max_still).
class ScreenMachineChecks{
public:
    //  A step that changes nothing on screen is a dead end (e.g. a menu the
    //  states do not know): fail after this many instead of running out the
    //  visit limit.
    static constexpr size_t MAX_UNCHANGED = 8;

    //  The problem with `state` coming up on `observation`, if any. The
    //  screen counts as unchanged only when it and the look before both have
    //  a frame and the frames match (same_screen()); how long, by the looks'
    //  `when`.
    std::optional<std::string> problem(const ScreenState& state, const ScreenObservation& observation);

private:
    //  The stall problem, if any, of `state` on a frame unchanged since
    //  `m_changed_at`.
    std::optional<std::string> unchanged_problem(const ScreenState& state, WallClock when);

    std::map<std::string, size_t, std::less<>> m_visits;
    VideoSnapshot m_last_frame;
    //  When the screen last changed (or was first looked at).
    WallClock m_changed_at = WallClock::min();
    size_t m_unchanged = 0;
};


std::optional<std::string> ScreenMachineChecks::unchanged_problem(const ScreenState& state, WallClock when){
    if (state.max_still > Milliseconds::zero()){
        if (when - m_changed_at > state.max_still){
            return std::format("The screen stayed still on \"{}\" for more than {}s.", state.name, state.max_still.count() / 1000);
        }
        return std::nullopt;
    }
    if (++m_unchanged >= MAX_UNCHANGED){
        return std::format("The screen stopped changing: {} steps on \"{}\" did nothing.", MAX_UNCHANGED, state.name);
    }
    return std::nullopt;
}

std::optional<std::string> ScreenMachineChecks::problem(const ScreenState& state, const ScreenObservation& observation){
    VideoSnapshot frame = observation.frame ? *observation.frame : VideoSnapshot();
    if (same_screen(m_last_frame, frame)){
        std::optional<std::string> stalled = unchanged_problem(state, observation.when);
        if (stalled){
            return stalled;
        }
    }else{
        m_unchanged = 0;
        m_changed_at = observation.when;
    }
    m_last_frame = std::move(frame);
    if (++m_visits[state.name] > state.max_visits){
        return std::format("Screen \"{}\" came up more than {} times: the step for it is not working.", state.name, state.max_visits);
    }
    return std::nullopt;
}


} // namespace



ScreenMachineResult run_screen_machine(
    const std::function<ScreenObservation()>& observe, CancellableScope& scope,
    const std::vector<ScreenState>& states, Milliseconds timeout, StepTrace& trace
){
    //  Looks before the run stops even when nothing else ends it (looks with
    //  no time, states with no visit limit).
    constexpr size_t MAX_LOOKS = 10000;
    ScreenMachineChecks checks;
    std::optional<WallClock> start;
    for (size_t looks = 0; looks < MAX_LOOKS; looks++){
        scope.throw_if_cancelled();
        const ScreenObservation observation = observe();
        if (!start){
            start = observation.when;
        }
        if (observation.when - *start > timeout){
            return {.terminal = "", .failure = std::format("Did not finish within {}s.", timeout.count() / 1000)};
        }
        const ScreenState* state = match_screen_state(states, observation);
        if (state == nullptr){
            return {.terminal = "", .failure = "The screen matches none of the known states."};
        }
        trace.screen(state->name, observation.when);
        std::optional<std::string> found = checks.problem(*state, observation);
        if (found){
            return {.terminal = "", .failure = std::move(*found)};
        }
        if (state->act){
            state->act();
        }
        if (state->terminal){
            return {.terminal = state->name, .failure = ""};
        }
    }
    return {.terminal = "", .failure = std::format("Did not finish within {} looks.", MAX_LOOKS)};
}



} // namespace PokemonAutomation
