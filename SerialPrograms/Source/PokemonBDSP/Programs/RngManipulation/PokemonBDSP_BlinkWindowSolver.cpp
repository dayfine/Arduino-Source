/*  BDSP Blink Window Solver
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include "PokemonBDSP_BlinkWindowSolver.h"
#include "PokemonBDSP_StateSolver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

BlinkWindowStep make_step(BlinkWindowStatus status, size_t events){
    BlinkWindowStep ret;
    ret.status = status;
    ret.events = events;
    return ret;
}

size_t count_events_and_longest(
    const std::vector<std::vector<BlinkEvent>>& streams,
    const std::vector<BlinkEvent>*& longest
){
    size_t events = 0;
    longest = &streams[0];
    for (const std::vector<BlinkEvent>& stream : streams){
        events += stream.size();
        if (stream.size() > longest->size()){
            longest = &stream;
        }
    }
    return events;
}

BlinkSolveResult try_solve_rotations(
    const std::vector<std::vector<BlinkEvent>>& streams,
    double period_seconds,
    std::vector<uint8_t>& winning_slots
){
    std::vector<uint8_t> slots(streams.size());
    for (uint8_t c = 0; c < (uint8_t)streams.size(); c++){
        slots[c] = c;
    }
    for (size_t attempt = 0; attempt < streams.size(); attempt++){
        std::vector<BlinkSample> samples;
        std::string failure;
        if (build_samples(streams, slots, period_seconds, samples, failure)){
            BlinkSolveResult solved = solve_state_from_samples(samples, nullptr);
            if (solved.success){
                winning_slots = std::move(slots);
                return solved;
            }
        }
        std::rotate(slots.begin(), slots.begin() + 1, slots.end());
    }
    return {};
}

} // namespace


BlinkWindowSolver::BlinkWindowSolver(const BlinkWindowSolverConfig& config)
    : m_config(config)
    , m_give_up_after(config.max_failed_windows * config.window_events)
{}

bool BlinkWindowSolver::should_trim_window() const{
    return !m_has_candidate;
}

bool BlinkWindowSolver::has_fresh_blink(
    const std::vector<std::vector<BlinkEvent>>& streams,
    WallClock origin
){
    if (m_newest_blink.size() < streams.size()){
        m_newest_blink.resize(streams.size(), WallClock::min());
    }
    bool fresh = false; // count doesn't change once the window is full
    for (size_t w = 0; w < streams.size(); w++){
        if (streams[w].empty()){
            continue;
        }
        WallClock newest = origin + std::chrono::duration_cast<WallDuration>(
            std::chrono::duration<double>(streams[w].back().seconds)
        );
        if (newest > m_newest_blink[w] + std::chrono::seconds(1)){
            fresh = true;
        }
        m_newest_blink[w] = std::max(m_newest_blink[w], newest);
    }
    return fresh;
}

BlinkWindowStep BlinkWindowSolver::handle_solve_failure(size_t events){
    m_failed_solves++;
    if (m_has_candidate){
        m_confirmation_failures++;
        if (m_config.max_confirmation_failures > 0
            && m_confirmation_failures >= m_config.max_confirmation_failures
        ){
            m_has_candidate = false;
            m_candidate = Pokemon::Xorshift128State();
            m_candidate_events = 0;
            m_confirmation_failures = 0;
            return make_step(
                m_failed_solves >= m_give_up_after
                    ? BlinkWindowStatus::GaveUp
                    : BlinkWindowStatus::ProvisionalDropped,
                events
            );
        }
    }
    return make_step(
        m_failed_solves >= m_give_up_after ? BlinkWindowStatus::GaveUp : BlinkWindowStatus::SolveFailed,
        events
    );
}

BlinkWindowStep BlinkWindowSolver::handle_solve_success(
    const Pokemon::Xorshift128State& state, std::vector<uint8_t> winning_slots,
    double tick_seconds, size_t events
){
    m_failed_solves = 0;
    m_confirmation_failures = 0;
    if (!m_has_candidate || state != m_candidate){
        BlinkWindowStatus status = m_has_candidate
            ? BlinkWindowStatus::ProvisionalChanged
            : BlinkWindowStatus::ProvisionalFound;
        m_has_candidate = true;
        m_candidate = state;
        m_candidate_events = events;
        BlinkWindowStep ret = make_step(status, events);
        ret.state = state;
        return ret;
    }
    return BlinkWindowStep{
        .status = BlinkWindowStatus::Confirmed,
        .events = events,
        .state = state,
        .slots = std::move(winning_slots),
        .tick_seconds = tick_seconds,
    };
}

BlinkWindowStep BlinkWindowSolver::step(const std::vector<std::vector<BlinkEvent>>& streams, WallClock origin){
    if (streams.empty()){
        return make_step(BlinkWindowStatus::Waiting, /*events=*/0);
    }
    const std::vector<BlinkEvent>* longest = nullptr;
    const size_t events = count_events_and_longest(streams, longest);
    if (m_failed_solves >= m_give_up_after){
        return make_step(BlinkWindowStatus::GaveUp, events);
    }
    if (!has_fresh_blink(streams, origin) || longest->size() < 3){
        return make_step(BlinkWindowStatus::Waiting, events);
    }
    const TickFit fit = fit_tick_period(*longest);
    if (!(fit.period_seconds > 0) || events < m_config.min_rolls_to_try){
        return make_step(BlinkWindowStatus::Waiting, events);
    }
    if (m_has_candidate && events < m_candidate_events + m_config.confirmation_events){
        return make_step(BlinkWindowStatus::Waiting, events);
    }
    std::vector<uint8_t> winning_slots;
    BlinkSolveResult solved = try_solve_rotations(streams, fit.period_seconds, winning_slots);
    return solved.success
        ? handle_solve_success(solved.state, std::move(winning_slots), fit.period_seconds, events)
        : handle_solve_failure(events);
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
