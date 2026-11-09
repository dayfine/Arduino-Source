/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
    if (!events.any_of(m_stop_on)){
        return false;
    }
    m_stopped_on.store(events.bits(), std::memory_order_release);
    return true;
}
void ScreenEventWatcher::reset(){
    m_seen.store(0, std::memory_order_release);
    m_stopped_on.store(0, std::memory_order_release);
}



} // namespace Pokemon
} // namespace PokemonAutomation
