/*  Covered Watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_CoveredWatch.h"

namespace PokemonAutomation{
namespace Pokemon{


CoveredWatcher::CoveredWatcher(
    VisualInferenceCallback& watcher, WallClock start, Milliseconds tail,
    std::optional<WallClock> end
)
    : VisualInferenceCallback(watcher.label())
    , m_watcher(watcher)
    , m_coverage(start)
    , m_tail(tail)
    , m_end(end)
{}

void CoveredWatcher::make_overlays(VideoOverlaySet& items) const{
    m_watcher.make_overlays(items);
}

bool CoveredWatcher::process_frame(const VideoSnapshot& frame){
    if (frame.timestamp < m_coverage.start()){
        return false;
    }
    m_coverage.record(frame.timestamp);
    if (m_watcher.process_frame(frame)){
        m_fired = true;
        return true;
    }
    return m_end && frame.timestamp >= *m_end + m_tail;
}


WatchResult watch_for(
    VideoStream& stream, CancellableScope& scope, Milliseconds duration,
    const WatchLimits& limits, VisualInferenceCallback& watcher
){
    const WallClock start = current_time();
    const WallClock end = start + duration;
    CoveredWatcher covered(watcher, start, limits.tail, end);
    wait_until(stream, scope, end + limits.tail + limits.cap, {covered}, limits.period);
    return judge_watch(covered.fired(), covered.coverage(), end, limits.max_gap);
}



} // namespace Pokemon
} // namespace PokemonAutomation
