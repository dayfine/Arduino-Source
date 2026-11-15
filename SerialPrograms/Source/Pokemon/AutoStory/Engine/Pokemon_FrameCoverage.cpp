/*  Frame Coverage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include "Pokemon_FrameCoverage.h"

namespace PokemonAutomation{
namespace Pokemon{


FrameCoverage::FrameCoverage(WallClock start)
    : m_start(start)
{}

void FrameCoverage::record(WallClock frame){
    if (frame < m_start){
        return;
    }
    if (!m_frames.empty() && frame <= m_frames.back()){
        return;
    }
    m_frames.push_back(frame);
}

std::optional<WallClock> FrameCoverage::last_frame() const{
    if (m_frames.empty()){
        return std::nullopt;
    }
    return m_frames.back();
}

bool FrameCoverage::reached(WallClock deadline) const{
    return !m_frames.empty() && m_frames.back() >= deadline;
}

Milliseconds FrameCoverage::largest_gap(WallClock end) const{
    WallDuration largest = WallDuration::zero();
    WallClock previous = m_start;
    for (WallClock frame : m_frames){
        const WallClock clamped = std::min(frame, end);
        largest = std::max(largest, clamped - previous);
        previous = clamped;
    }
    largest = std::max(largest, end - previous);
    return std::chrono::ceil<Milliseconds>(largest);
}


WatchResult judge_watch(bool fired, const FrameCoverage& coverage, WallClock end, Milliseconds max_gap){
    WatchResult result;
    result.frames = coverage.frames();
    result.largest_gap = coverage.largest_gap(end);
    if (fired){
        result.verdict = WatchVerdict::SEEN;
    }else if (result.largest_gap <= max_gap){
        result.verdict = WatchVerdict::NOT_SEEN;
    }else{
        result.verdict = WatchVerdict::UNKNOWN;
    }
    return result;
}

std::string describe(WatchVerdict verdict){
    switch (verdict){
    case WatchVerdict::SEEN:
        return "seen";
    case WatchVerdict::NOT_SEEN:
        return "not seen";
    case WatchVerdict::UNKNOWN:
        return "unknown";
    }
    return "unknown";
}

std::string describe(const WatchResult& result){
    const double gap_seconds = result.largest_gap.count() / 1000.0;
    if (result.verdict == WatchVerdict::UNKNOWN){
        return std::format("unknown: largest gap between frames {:.2f} s over {} frames", gap_seconds, result.frames);
    }
    return std::format("{} over {} frames, largest gap {:.2f} s", describe(result.verdict), result.frames, gap_seconds);
}



} // namespace Pokemon
} // namespace PokemonAutomation
