/*  Screen Event Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_ScreenEvents.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenEventTypes_H
#define PokemonAutomation_Pokemon_ScreenEventTypes_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <string>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One thing a frame shows. A game reports the ones its detectors can see.
enum class ScreenEvent : uint8_t{
    FADE_BLACK,         //  A uniform dark screen: a door, a cave mouth, a scene cut.
    FADE_WHITE,         //  A uniform white screen: a cave exit.
    LOCATION_BANNER,    //  A location banner. An observer given an expected
                        //  map reports only that map's.
    DIALOG_BOX,         //  A dialogue box.
    CHOICE_BOX,         //  A choice (Yes/No, a menu of answers).
    INTERACT_PROMPT,    //  A button prompt to interact ("A Talk").
    BATTLE_STARTED,     //  A battle shows (its menu, its HUD or its opening).
    BATTLE_ENDED,       //  A battle's closing screen.
    PLAYER_CAN_MOVE,    //  The overworld with the player in control.
    SCREEN_STILL,       //  The screen barely changed since the frame before.
    SWITCH_HOME_MENU,   //  The Switch's HOME menu: the game is not on the screen.
};
//  How many ScreenEvent values there are.
constexpr size_t SCREEN_EVENT_COUNT = 11;
//  "dialog box", for the log.
std::string screen_event_name(ScreenEvent event);


//  How a screen's fade looks: the kind of FADE_BLACK / FADE_WHITE.
enum class WarpFade{
    NONE,
    BLACK,      //  a door's black, or a cave mouth's dim fade
    WHITE,      //  a cave exit's white, with no battle after it
};
//  "black fade", for the log.
std::string warp_fade_name(WarpFade fade);


//  The events of one frame, or of all the frames a watcher saw.
class ScreenEvents{
public:
    ScreenEvents() = default;
    ScreenEvents(std::initializer_list<ScreenEvent> events);

    bool has(ScreenEvent event) const{
        return (m_bits & bit(event)) != 0;
    }
    //  Whether any of `events` is in this set.
    bool any_of(const ScreenEvents& events) const{
        return (m_bits & events.m_bits) != 0;
    }
    //  Adds `event`, or removes it when `on` is false (as std::bitset::set).
    void set(ScreenEvent event, bool on);
    void merge(const ScreenEvents& events){
        m_bits |= events.m_bits;
    }

    uint32_t bits() const{
        return m_bits;
    }
    static ScreenEvents from_bits(uint32_t bits);

    bool operator==(const ScreenEvents& other) const = default;

private:
    static uint32_t bit(ScreenEvent event){
        return uint32_t(1) << (uint32_t)event;
    }

    uint32_t m_bits = 0;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
