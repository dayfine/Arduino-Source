/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "ScreenEventTypes.h"
#include "ScreenFades.h"
#include "ScreenEvents.h"

namespace PokemonAutomation{


std::string screen_event_name(ScreenEvent event){
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

std::string warp_fade_name(WarpFade fade){
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
std::string describe(const ScreenEvents& events, std::string_view separator){
    std::string out;
    for (size_t c = 0; c < SCREEN_EVENT_COUNT; c++){
        const ScreenEvent event = (ScreenEvent)c;
        if (!events.has(event)){
            continue;
        }
        if (!out.empty()){
            out += separator;
        }
        out += screen_event_name(event);
    }
    return out.empty() ? "nothing" : out;
}


//  Black is is_black() (SolidColorTest, BlackScreenDetector's test) with
//  the fade's thresholds.
//  White is not is_white(): that also wants an even grey, and a fade to
//  white can be tinted.
WarpFade fade_of(const ImageStats& middle){
    if (is_black(middle, FADE_MAX_DARK_SUM, FADE_MAX_STDDEV_SUM)){
        return WarpFade::BLACK;
    }
    if (middle.stddev.sum() > FADE_MAX_STDDEV_SUM){
        return WarpFade::NONE;
    }
    return middle.average.sum() >= FADE_MIN_WHITE_SUM ? WarpFade::WHITE : WarpFade::NONE;
}
WarpFade fade_of_screen(const ImageViewRGB32& screen){
    return fade_of(image_stats(extract_box_reference(screen, ImageFloatBox(0.1, 0.1, 0.8, 0.8))));
}


bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b){
    if (!a || !b || a->width() != b->width() || a->height() != b->height()){
        return false;
    }
    return ImageMatch::pixel_RMSD(a, b) < 2.0;
}


}  // namespace PokemonAutomation
