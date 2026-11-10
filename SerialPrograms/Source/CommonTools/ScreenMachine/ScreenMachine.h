/*  Screen Machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A closed loop over known screens: read the screen, take the one step that screen
 *  calls for, read again. Every step follows a read, a screen counts only once two
 *  frames agree, and each state lists the screens it may lead to, so a dropped or
 *  wrong input shows up as an unexpected screen or a state that keeps coming back.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenMachine_H
#define PokemonAutomation_CommonTools_ScreenMachine_H

#include <stdint.h>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/CancellableScope.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "ScreenObservation.h"
#include "StepTrace.h"

namespace PokemonAutomation{


// Single recognizable screen state, including detection predicate and handler action.
struct ScreenState{
    std::string name;
    //  Whether this is the screen. States are tried in order; the first match wins, so
    //  the last state should match anything.
    std::function<bool(const ImageViewRGB32& screen)> matches;
    //  Whether this is the screen, from what a look at the game observed
    //  instead of from its pixels, so the state also runs in a simulation.
    //  A state may have either or both; read_screen_state() uses `matches`
    //  and skips a state without it.
    std::function<bool(const ScreenObservation& observation)> matches_observation = nullptr;
    //  The one step to take on this screen.
    std::function<void()> act;
    //  The screens the step may lead to besides staying here. Empty: any.
    std::vector<std::string> next;
    //  Fail once the screen has been seen this many times.
    size_t max_visits;
    //  End the run after this state's step.
    bool terminal = false;
    //  Fail once the screen has been seen this many times in a row.
    size_t max_in_a_row = SIZE_MAX;
    //  How long the screen may stay unchanged on this state. A waiting
    //  state (a fade, a still cutscene, a hold before a screen counts) sets
    //  it: its looks never count as a stall, and it fails only once the
    //  screen has not changed for longer than this. Zero: the state's step
    //  is input that should change the screen, and MAX_UNCHANGED such
    //  steps that do not are a stall.
    Milliseconds max_still = Milliseconds::zero();
};


//  The state on screen, once two frames 150ms apart agree. Waits out a video
//  gap of up to 10s. Fails through `trace` if no state matches.
const ScreenState& read_screen_state(
    VideoStream& stream, CancellableScope& scope, StepTrace& trace,
    const std::vector<ScreenState>& states, VideoSnapshot& frame
);

//  The first of `states` whose matches_observation() accepts `observation`;
//  null when none does. States without one are skipped.
const ScreenState* match_screen_state(const std::vector<ScreenState>& states, const ScreenObservation& observation);

//  The checks between reads: the screen must be one the previous state can lead
//  to, no state may come up too often, and steps must keep changing the screen
//  (a waiting state's only for its ScreenState::max_still).
class ScreenMachineChecks{
public:
    ScreenMachineChecks(StepTrace& trace)
        : m_trace(trace)
    {}
    //  Fails through the trace on a problem.
    void check(const ScreenState& state, VideoSnapshot frame);
    //  The problem with `state` coming up on `observation`, if any. The
    //  screen counts as unchanged only when it and the look before both have
    //  a frame and the frames match; how long, by the looks' `when`.
    std::optional<std::string> problem(const ScreenState& state, const ScreenObservation& observation);

private:
    std::optional<std::string> problem(const ScreenState& state, VideoSnapshot frame, WallClock when);
    //  The stall problem, if any, of `state` on a frame unchanged since `m_changed_at`.
    std::optional<std::string> unchanged_problem(const ScreenState& state, WallClock when);

    //  A step that changes nothing on screen is a dead end (e.g. a menu the states do
    //  not know): fail after a few instead of running out the visit limit.
    static constexpr size_t MAX_UNCHANGED = 8;

    StepTrace& m_trace;
    std::map<std::string, size_t> m_visits;
    const ScreenState* m_previous = nullptr;
    VideoSnapshot m_last_frame;
    //  When the screen last changed (or was first read).
    WallClock m_changed_at = WallClock::min();
    size_t m_unchanged = 0;
    size_t m_in_a_row = 0;
};


//  Runs the states until a terminal one has acted. Returns its name. Fails through
//  `trace` on an unexpected screen, a state seen too often, a stall or the timeout.
template <typename ControllerContext>
std::string run_screen_machine(
    VideoStream& stream, ControllerContext& context, StepTrace& trace,
    const std::vector<ScreenState>& states, Milliseconds timeout
);


//  How a run of states over observations ended.
struct ScreenMachineResult{
    //  The terminal state that acted; empty when the run failed.
    std::string terminal;
    //  Why it failed: no state matched, an unexpected screen, a state seen too
    //  often, a stall, or the timeout.
    std::string failure;

    bool succeeded() const{
        return failure.empty();
    }
};

//  The same loop as run_screen_machine() over looks from `observe` instead of
//  frames, so it runs on a console and in a simulation alike: each look picks
//  the first state whose matches_observation() accepts it (one look, no second
//  read: an observation is already settled), the trace records it, the checks
//  apply, then the state acts. Time is the looks' own (`when`): the run fails
//  once `timeout` has passed since the first look, and after 10000 looks in
//  any case. A failure is returned, not thrown.
ScreenMachineResult run_screen_machine_on_observations(
    const std::function<ScreenObservation()>& observe, CancellableScope& scope,
    const std::vector<ScreenState>& states, Milliseconds timeout, StepTrace& trace
);

template <typename ControllerContext>
std::string run_screen_machine(
    VideoStream& stream, ControllerContext& context, StepTrace& trace,
    const std::vector<ScreenState>& states, Milliseconds timeout
){
    ScreenMachineChecks checks(trace);
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        VideoSnapshot frame;
        const ScreenState& state = read_screen_state(stream, context, trace, states, frame);
        trace.screen(state.name);
        checks.check(state, std::move(frame));
        state.act();
        if (state.terminal){
            context.wait_for_all_requests();
            return state.name;
        }
    }
    trace.fail("Did not finish within " + std::to_string(timeout.count() / 1000) + "s.");
}



} // namespace PokemonAutomation
#endif
