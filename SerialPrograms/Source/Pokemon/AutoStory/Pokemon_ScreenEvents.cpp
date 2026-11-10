/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon_ScreenEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* screen_event_name(ScreenEvent event){
    switch (event){
    case ScreenEvent::FADE_BLACK:       return "black fade";
    case ScreenEvent::FADE_WHITE:       return "white fade";
    case ScreenEvent::LOCATION_BANNER:  return "location banner";
    case ScreenEvent::DIALOG_BOX:       return "dialog box";
    case ScreenEvent::CHOICE_BOX:       return "choice box";
    case ScreenEvent::INTERACT_PROMPT:  return "interact prompt";
    case ScreenEvent::BATTLE_STARTED:   return "battle started";
    case ScreenEvent::BATTLE_ENDED:     return "battle ended";
    case ScreenEvent::PLAYER_CAN_MOVE:  return "player can move";
    case ScreenEvent::SCREEN_STILL:     return "screen still";
    case ScreenEvent::SWITCH_HOME_MENU: return "Switch HOME menu";
    }
    return "?";
}

const char* warp_fade_name(WarpFade fade){
    switch (fade){
    case WarpFade::NONE:    return "no fade";
    case WarpFade::BLACK:   return "black fade";
    case WarpFade::WHITE:   return "white fade";
    }
    return "?";
}


ScreenEvents::ScreenEvents(std::initializer_list<ScreenEvent> events){
    for (ScreenEvent event : events){
        set(event, true);
    }
}
void ScreenEvents::set(ScreenEvent event, bool on){
    if (on){
        m_bits |= bit(event);
    }else{
        m_bits &= ~bit(event);
    }
}
ScreenEvents ScreenEvents::from_bits(uint32_t bits){
    ScreenEvents out;
    out.m_bits = bits & ((uint32_t(1) << SCREEN_EVENT_COUNT) - 1);
    return out;
}

ScreenEvents fade_events(WarpFade fade){
    switch (fade){
    case WarpFade::NONE:
        return {};
    case WarpFade::BLACK:
        return {ScreenEvent::FADE_BLACK};
    case WarpFade::WHITE:
        return {ScreenEvent::FADE_WHITE};
    }
    return {};
}
WarpFade fade_in(const ScreenEvents& events){
    if (events.has(ScreenEvent::FADE_BLACK)){
        return WarpFade::BLACK;
    }
    if (events.has(ScreenEvent::FADE_WHITE)){
        return WarpFade::WHITE;
    }
    return WarpFade::NONE;
}
std::string describe(const ScreenEvents& events){
    std::string out;
    for (size_t c = 0; c < SCREEN_EVENT_COUNT; c++){
        const ScreenEvent event = (ScreenEvent)c;
        if (!events.has(event)){
            continue;
        }
        if (!out.empty()){
            out += ", ";
        }
        out += screen_event_name(event);
    }
    return out.empty() ? "nothing" : out;
}


WarpFade fade_of(const ImageStats& middle){
    if (middle.stddev.sum() > FADE_MAX_STDDEV_SUM){
        return WarpFade::NONE;
    }
    const double sum = middle.average.sum();
    return sum <= FADE_MAX_DARK_SUM ? WarpFade::BLACK : sum >= FADE_MIN_WHITE_SUM ? WarpFade::WHITE : WarpFade::NONE;
}
WarpFade fade_of_screen(const ImageViewRGB32& screen){
    return fade_of(image_stats(extract_box_reference(screen, ImageFloatBox(0.1, 0.1, 0.8, 0.8))));
}


ScreenEventLog::ScreenEventLog(size_t capacity)
    : m_capacity(capacity)
{}
void ScreenEventLog::record(const ScreenEvents& frame, WallClock now){
    const uint32_t changed = frame.bits() ^ m_showing.bits();
    for (size_t c = 0; c < SCREEN_EVENT_COUNT; c++){
        if ((changed & (uint32_t(1) << c)) == 0){
            continue;
        }
        const ScreenEvent event = (ScreenEvent)c;
        m_changes.push_back({now, event, frame.has(event) ? ScreenEventEdge::ROSE : ScreenEventEdge::FELL});
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
            screen_event_name(change.event),
            change.edge == ScreenEventEdge::ROSE ? "rose" : "fell"
        );
    }
    return out.empty() ? "no changes" : out;
}


SwitchHomeObserver::SwitchHomeObserver(NintendoSwitch::ConsoleHandle& console, ScreenEventObserver& game)
    : m_game(game)
    , m_home(console, COLOR_RED)
{}

ScreenEvents SwitchHomeObserver::observe(const VideoSnapshot& frame, WallClock now){
    if (frame && m_home.detect_only(*frame.frame)){
        return {ScreenEvent::SWITCH_HOME_MENU};
    }
    return m_game.observe(frame, now);
}

void SwitchHomeObserver::make_overlays(VideoOverlaySet& items) const{
    m_game.make_overlays(items);
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
    m_seen.fetch_or(events.bits(), std::memory_order_acq_rel);
    {
        LockGuard<Mutex> lg(m_log_lock);
        m_log.record(events, frame.timestamp);
    }
    if (!events.any_of(m_stop_on)){
        return false;
    }
    m_stopped_on.store(events.bits(), std::memory_order_release);
    return true;
}
ScreenEventLog ScreenEventWatcher::log() const{
    LockGuard<Mutex> lg(m_log_lock);
    return m_log;
}
void ScreenEventWatcher::reset(){
    m_seen.store(0, std::memory_order_release);
    m_stopped_on.store(0, std::memory_order_release);
    LockGuard<Mutex> lg(m_log_lock);
    m_log.clear();
}



} // namespace Pokemon
} // namespace PokemonAutomation
