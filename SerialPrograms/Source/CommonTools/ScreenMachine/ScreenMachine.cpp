/*  Screen Machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "ScreenMachine.h"

namespace PokemonAutomation{

using namespace std::chrono_literals;


namespace{

const ScreenState* classify(const std::vector<ScreenState>& states, const ImageViewRGB32& screen){
    for (const ScreenState& state : states){
        if (state.matches && state.matches(screen)){
            return &state;
        }
    }
    return nullptr;
}

} // namespace


std::string_view screen_event_name(ScreenEvent event){
    switch (event){
    case ScreenEvent::FADE_BLACK:       return "fade_black";
    case ScreenEvent::FADE_WHITE:       return "fade_white";
    case ScreenEvent::TEXT_BOX:         return "text_box";
    case ScreenEvent::BATTLE:           return "battle";
    case ScreenEvent::CAN_MOVE:         return "can_move";
    case ScreenEvent::STILL:            return "still";
    case ScreenEvent::CHOICE_BOX:       return "choice_box";
    case ScreenEvent::INTERACT_PROMPT:  return "interact_prompt";
    }
    return "unknown";
}

std::string describe(const ScreenEvents& events, std::string_view separator){
    constexpr ScreenEvent ALL[] = {
        ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE, ScreenEvent::TEXT_BOX,
        ScreenEvent::BATTLE, ScreenEvent::CAN_MOVE, ScreenEvent::STILL,
        ScreenEvent::CHOICE_BOX, ScreenEvent::INTERACT_PROMPT,
    };
    std::string text;
    for (ScreenEvent event : ALL){
        if (events.has(event)){
            text += text.empty() ? "" : separator;
            text += screen_event_name(event);
        }
    }
    return text.empty() ? std::string("nothing") : text;
}

bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b){
    if (!a || !b || a->width() != b->width() || a->height() != b->height()){
        return false;
    }
    return ImageMatch::pixel_RMSD(a, b) < 2.0;
}

const ScreenState* match_screen_state(const std::vector<ScreenState>& states, const ScreenObservation& observation){
    for (const ScreenState& state : states){
        if (state.matches_observation && state.matches_observation(observation)){
            return &state;
        }
    }
    return nullptr;
}


const ScreenState& read_screen_state(
    VideoStream& stream, CancellableScope& scope,
    const std::vector<ScreenState>& states, StepTrace& trace, VideoSnapshot& frame
){
    const ScreenState* last = nullptr;
    WallClock video_lost = WallClock::max();
    for (size_t c = 0; c < 20; c++){
        VideoSnapshot screen = stream.video().snapshot();
        if (!screen){
            //  The app resets a frozen capture by itself, which takes a second or two.
            const WallClock now = current_time();
            video_lost = std::min(video_lost, now);
            if (now - video_lost > std::chrono::seconds(10)){
                trace.fail("No video from the capture card for 10s.");
            }
            c--;
            scope.wait_for(500ms);
            continue;
        }
        video_lost = WallClock::max();
        const ScreenState* now = classify(states, screen);
        frame = std::move(screen);
        if (now != nullptr && now == last){
            return *now;
        }
        last = now;
        scope.wait_for(150ms);
    }
    if (last == nullptr){
        trace.fail("The screen matches none of the known states.");
    }
    return *last;   //  Still changing after 3s: go with the latest read.
}


void ScreenMachineChecks::check(const ScreenState& state, VideoSnapshot frame){
    const std::optional<std::string> found = problem(state, std::move(frame), current_time());
    if (found){
        m_trace.fail(*found);
    }
}

std::optional<std::string> ScreenMachineChecks::problem(const ScreenState& state, const ScreenObservation& observation){
    return problem(state, observation.frame ? *observation.frame : VideoSnapshot(), observation.when);
}

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

std::optional<std::string> ScreenMachineChecks::problem(const ScreenState& state, VideoSnapshot frame, WallClock when){
    if (same_screen(m_last_frame, frame)){
        std::optional<std::string> stalled = unchanged_problem(state, when);
        if (stalled){
            return stalled;
        }
    }else{
        m_unchanged = 0;
        m_changed_at = when;
    }
    m_last_frame = std::move(frame);

    if (m_previous != nullptr && m_previous != &state && !m_previous->next.empty() &&
        std::find(m_previous->next.begin(), m_previous->next.end(), state.name) == m_previous->next.end()
    ){
        return std::format("Unexpected screen after \"{}\": \"{}\".", m_previous->name, state.name);
    }
    m_in_a_row = m_previous == &state ? m_in_a_row + 1 : 1;
    if (m_in_a_row > state.max_in_a_row){
        return std::format("Screen \"{}\" came up {} times in a row: the step for it is not getting anywhere.", state.name, m_in_a_row);
    }
    if (++m_visits[state.name] > state.max_visits){
        return std::format("Screen \"{}\" came up more than {} times: the step for it is not working.", state.name, state.max_visits);
    }
    m_previous = &state;
    return std::nullopt;
}


ScreenMachineResult run_screen_machine_on_observations(
    const std::function<ScreenObservation()>& observe, CancellableScope& scope,
    const std::vector<ScreenState>& states, Milliseconds timeout, StepTrace& trace
){
    //  Looks before the run stops even when nothing else ends it (looks with
    //  no time, states with no visit limit).
    constexpr size_t MAX_LOOKS = 10000;
    ScreenMachineChecks checks(trace);
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
        trace.screen(state->name);
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
