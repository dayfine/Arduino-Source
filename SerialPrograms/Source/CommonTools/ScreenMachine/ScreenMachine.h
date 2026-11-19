/*  Screen Machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A closed loop over known screens: look, take that screen's step, look again.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenMachine_H
#define PokemonAutomation_CommonTools_ScreenMachine_H

#include <stdint.h>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/CancellableScope.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "ScreenObservation.h"

namespace PokemonAutomation{


//  One recognizable screen: how to tell it, and the step to take on it.
struct ScreenState{
    std::string name = {};
    //  Whether a look shows this screen; the first matching state wins.
    std::function<bool(const ScreenObservation& observation)> matches = nullptr;
    //  The one step to take on this screen; none for a state that only
    //  waits.
    std::function<void()> act = nullptr;
    //  Fail once the screen has been seen more than this many times.
    size_t max_visits = SIZE_MAX;
    //  End the run after this state's step.
    bool terminal = false;
    //  How long a waiting state may leave the screen unchanged. Zero: its
    //  step should change the screen, and repeated steps that don't stall.
    Milliseconds max_still = Milliseconds::zero();
};


//  The run succeeded: this terminal state acted.
struct ScreenMachineTerminal{
    std::string state_name = {};
};
//  The run failed before a terminal state acted.
struct ScreenMachineFailure{
    std::string reason = {};
};
//  How a run of states ended.
using ScreenMachineResult = std::variant<ScreenMachineTerminal, ScreenMachineFailure>;

//  Runs `states` over looks from `observe` until a terminal one has acted,
//  or fails after `timeout` of look time. Logs each change of state to `log`
//  when set.
ScreenMachineResult run_screen_machine(
    const std::function<ScreenObservation()>& observe, CancellableScope& scope,
    const std::vector<ScreenState>& states, Milliseconds timeout,
    const std::function<void(std::string_view line)>& log
);



} // namespace PokemonAutomation
#endif
