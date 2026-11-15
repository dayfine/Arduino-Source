/*  Covered Watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_CoveredWatch.h"

namespace PokemonAutomation{
namespace Pokemon{


CoveredWatcher::CoveredWatcher(VisualInferenceCallback& watcher, WallClock start)
    : VisualInferenceCallback(watcher.label())
    , m_watcher(watcher)
    , m_coverage(start)
{}

void CoveredWatcher::make_overlays(VideoOverlaySet& items) const{
    m_watcher.make_overlays(items);
}

bool CoveredWatcher::process_frame(const VideoSnapshot& frame){
    m_coverage.record(frame.timestamp);
    if (m_watcher.process_frame(frame)){
        m_fired = true;
        return true;
    }
    return m_stop_at && frame.timestamp >= *m_stop_at;
}


WatchResult watch_for(
    VideoStream& stream, CancellableScope& scope, Milliseconds duration,
    Milliseconds max_gap, Milliseconds cap, VisualInferenceCallback& watcher
){
    const WallClock start = current_time();
    const WallClock end = start + duration;
    CoveredWatcher covered(watcher, start);
    covered.stop_at(end);
    wait_until(stream, scope, end + cap, {covered});
    return judge_watch(covered.fired(), covered.coverage(), end, max_gap);
}



} // namespace Pokemon
} // namespace PokemonAutomation
