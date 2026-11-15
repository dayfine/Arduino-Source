/*  Screen Machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A closed loop over known screens: look at the game, take the one step that
 *  screen calls for, look again. Every step follows a look, and each state says
 *  how often it may come up and how long the screen may stay unchanged on it,
 *  so a dropped or wrong input shows up as a state that keeps coming back or a
 *  screen that stops changing. The looks are ScreenObservations, so the same
 *  machine runs on a console and on a scripted backend.
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
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "ScreenObservation.h"
#include "StepTrace.h"

namespace PokemonAutomation{


//  One recognizable screen: how to tell it, and the step to take on it.
struct ScreenState{
    std::string name;
    //  Whether a look shows this screen. States are tried in order; the first
    //  match wins.
    std::function<bool(const ScreenObservation& observation)> matches;
    //  The one step to take on this screen; none for a state that only
    //  waits.
    std::function<void()> act = nullptr;
    //  Fail once the screen has been seen more than this many times.
    size_t max_visits = SIZE_MAX;
    //  End the run after this state's step.
    bool terminal = false;
    //  How long the screen may stay unchanged on this state. A waiting
    //  state (a fade, a still cutscene, a hold before a screen counts) sets
    //  it: its looks never count as a stall, and it fails only once the
    //  screen has not changed for longer than this. Zero: the state's step
    //  is input that should change the screen, and
    //  ScreenMachineChecks::MAX_UNCHANGED such steps that do not are a stall.
    Milliseconds max_still = Milliseconds::zero();
};


//  The first of `states` whose matches() accepts `observation`; null when
//  none does.
const ScreenState* match_screen_state(const std::vector<ScreenState>& states, const ScreenObservation& observation);


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


//  How a run of states ended.
struct ScreenMachineResult{
    //  The terminal state that acted; empty when the run failed.
    std::string terminal;
    //  Why it failed: no state matched, a state seen too often, a stall, or
    //  the timeout.
    std::string failure;

    bool succeeded() const{
        return failure.empty();
    }
};

//  Runs `states` over looks from `observe` until a terminal one has acted:
//  each look picks the first state that matches it, `trace` records it, the
//  checks apply, then the state acts. Time is the looks' own (`when`): the
//  run fails once `timeout` has passed since the first look, and after
//  10000 looks in any case. A failure is returned, not thrown.
ScreenMachineResult run_screen_machine(
    const std::function<ScreenObservation()>& observe, CancellableScope& scope,
    const std::vector<ScreenState>& states, Milliseconds timeout, StepTrace& trace
);



} // namespace PokemonAutomation
#endif
