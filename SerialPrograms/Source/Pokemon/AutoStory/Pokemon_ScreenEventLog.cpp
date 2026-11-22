/*  Screen Event Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include "Pokemon_ScreenEventLog.h"

namespace PokemonAutomation{


ScreenEventLog::ScreenEventLog(size_t capacity)
    : m_capacity(capacity)
{}

void ScreenEventLog::record(const ScreenEvents& frame, WallClock now){
    for (const auto& [event, unused] : SCREEN_EVENT_NAMES()){
        const bool was = m_showing.has(event);
        const bool is = frame.has(event);
        if (was == is){
            continue;
        }
        m_changes.push_back({now, event, is ? ScreenEventEdge::ROSE : ScreenEventEdge::FELL});
    }
    while (m_changes.size() > m_capacity){
        m_changes.pop_front();
        m_dropped++;
    }
    m_showing = frame;
    m_last_frame = now;
}

void ScreenEventLog::clear(){
    m_changes.clear();
    m_dropped = 0;
    m_showing = ScreenEvents();
    m_last_frame.reset();
}

std::string describe(const ScreenEventLog& log, WallClock origin){
    std::string out;
    if (log.dropped() > 0){
        out += std::format("({} older changes dropped)\n", log.dropped());
    }
    for (const ScreenEventChange& change : log.changes()){
        out += std::format(
            "{:+} ms {} {}\n",
            std::chrono::duration_cast<Milliseconds>(change.time - origin).count(),
            SCREEN_EVENT_NAMES().get_string(change.event),
            change.edge == ScreenEventEdge::ROSE ? "rose" : "fell"
        );
    }
    return out.empty() ? "no changes" : out;
}


ScreenEventWatcher::ScreenEventWatcher(ScreenEventObserver& observer, ScreenEvents stop_on)
    : VisualInferenceCallback("ScreenEventWatcher")
    , m_observer(observer)
    , m_stop_on(stop_on)
{}

void ScreenEventWatcher::make_overlays(VideoOverlaySet& items) const{
    m_observer.make_overlays(items);
}

bool ScreenEventWatcher::process_frame(const VideoSnapshot& frame){
    const ScreenEvents events = m_observer.observe(frame, frame.timestamp);
    const bool stop = events.any_of(m_stop_on);
    {
        LockGuard<Mutex> lg(m_log_lock);
        m_seen.merge(events);
        m_log.record(events, frame.timestamp);
        if (stop){
            m_stopped_on = events;
        }
    }
    return stop;
}

ScreenEvents ScreenEventWatcher::seen() const{
    LockGuard<Mutex> lg(m_log_lock);
    return m_seen;
}

ScreenEvents ScreenEventWatcher::stopped_on() const{
    LockGuard<Mutex> lg(m_log_lock);
    return m_stopped_on;
}

ScreenEventLog ScreenEventWatcher::log() const{
    LockGuard<Mutex> lg(m_log_lock);
    return m_log;
}

void ScreenEventWatcher::reset(){
    LockGuard<Mutex> lg(m_log_lock);
    m_seen = ScreenEvents();
    m_stopped_on = ScreenEvents();
    m_log.clear();
}



}  // namespace PokemonAutomation
