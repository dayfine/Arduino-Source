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
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/CancellableScope.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "StepTrace.h"

namespace PokemonAutomation{


struct ScreenState{
    std::string name;
    //  Whether this is the screen. States are tried in order; the first match wins, so
    //  the last state should match anything.
    std::function<bool(const ImageViewRGB32& screen)> matches;
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
};


//  The state on screen, once two frames 150ms apart agree. Waits out a video
//  gap of up to 10s. Fails through `trace` if no state matches.
const ScreenState& read_screen_state(
    VideoStream& stream, CancellableScope& scope, StepTrace& trace,
    const std::vector<ScreenState>& states, VideoSnapshot& frame
);

//  The checks between reads: the screen must be one the previous state can lead
//  to, no state may come up too often, and steps must keep changing the screen.
class ScreenMachineChecks{
public:
    ScreenMachineChecks(StepTrace& trace)
        : m_trace(trace)
    {}
    void check(const ScreenState& state, VideoSnapshot frame);

private:
    //  A step that changes nothing on screen is a dead end (e.g. a menu the states do
    //  not know): fail after a few instead of running out the visit limit.
    static constexpr size_t MAX_UNCHANGED = 8;

    StepTrace& m_trace;
    std::map<std::string, size_t> m_visits;
    const ScreenState* m_previous = nullptr;
    VideoSnapshot m_last_frame;
    size_t m_unchanged = 0;
    size_t m_in_a_row = 0;
};


//  Runs the states until a terminal one has acted. Returns its name. Fails through
//  `trace` on an unexpected screen, a state seen too often, a stall or the timeout.
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



}
#endif
