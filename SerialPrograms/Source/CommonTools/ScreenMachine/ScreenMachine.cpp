/*  Screen Machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "ScreenMachine.h"

namespace PokemonAutomation{

using namespace std::chrono_literals;


namespace{

const ScreenState* classify(const std::vector<ScreenState>& states, const ImageViewRGB32& screen){
    for (const ScreenState& state : states){
        if (state.matches(screen)){
            return &state;
        }
    }
    return nullptr;
}

} // namespace


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
    const bool same_size = m_last_frame && frame &&
        m_last_frame->width() == frame->width() && m_last_frame->height() == frame->height();
    if (same_size && ImageMatch::pixel_RMSD(m_last_frame, frame) < 2.0){
        if (++m_unchanged >= MAX_UNCHANGED){
            m_trace.fail(std::format(
                "The screen stopped changing: {} steps on \"{}\" did nothing.", MAX_UNCHANGED, state.name
            ));
        }
    }else{
        m_unchanged = 0;
    }
    m_last_frame = std::move(frame);

    if (m_previous != nullptr && m_previous != &state && !m_previous->next.empty() &&
        std::find(m_previous->next.begin(), m_previous->next.end(), state.name) == m_previous->next.end()
    ){
        m_trace.fail(std::format("Unexpected screen after \"{}\": \"{}\".", m_previous->name, state.name));
    }
    m_in_a_row = m_previous == &state ? m_in_a_row + 1 : 1;
    if (m_in_a_row > state.max_in_a_row){
        m_trace.fail(std::format(
            "Screen \"{}\" came up {} times in a row: the step for it is not getting anywhere.",
            state.name, m_in_a_row
        ));
    }
    if (++m_visits[state.name] > state.max_visits){
        m_trace.fail(std::format(
            "Screen \"{}\" came up more than {} times: the step for it is not working.",
            state.name, state.max_visits
        ));
    }
    m_previous = &state;
}



} // namespace PokemonAutomation
