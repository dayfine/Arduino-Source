/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a story step can see on the screen, in words every game shares: a
 *  fade, a location banner, a dialogue box, a battle starting. Each game
 *  implements ScreenEventObserver with its own detectors; the judgements
 *  built on these events (Pokemon_SceneJudges.h) are then the same for all
 *  of them.
 *
 *  An observer runs during a controller action as a ScreenEventWatcher
 *  (a VisualInferenceCallback for run_until), or is asked frame by frame.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenEvents_H
#define PokemonAutomation_Pokemon_ScreenEvents_H

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"

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
};
constexpr size_t SCREEN_EVENT_COUNT = 10;
//  "dialog box", for the log.
const char* screen_event_name(ScreenEvent event);


//  How a screen's fade looks: the kind of FADE_BLACK / FADE_WHITE.
enum class WarpFade{
    NONE,
    BLACK,      //  a door's black, or a cave mouth's dim fade
    WHITE,      //  a cave exit's white, with no battle after it
};
//  "black fade", for the log.
const char* warp_fade_name(WarpFade fade);


//  The events of one frame, or of all the frames a watcher saw.
class ScreenEvents{
public:
    ScreenEvents() = default;
    ScreenEvents(std::initializer_list<ScreenEvent> events);

    bool has(ScreenEvent event) const{
        return (m_bits & bit(event)) != 0;
    }
    bool empty() const{
        return m_bits == 0;
    }
    //  Whether any of `events` is in this set.
    bool any_of(const ScreenEvents& events) const{
        return (m_bits & events.m_bits) != 0;
    }
    //  Adds `event`, or removes it when `on` is false (as std::bitset::set).
    void set(ScreenEvent event, bool on = true);
    //  Adds the fade's event (nothing for WarpFade::NONE).
    void add_fade(WarpFade fade);
    //  The fade this set shows: BLACK before WHITE if it has both.
    WarpFade fade() const;
    void merge(const ScreenEvents& events){
        m_bits |= events.m_bits;
    }

    uint32_t bits() const{
        return m_bits;
    }
    static ScreenEvents from_bits(uint32_t bits);

    //  "dialog box, player can move"; "nothing" when empty.
    std::string describe() const;

    bool operator==(const ScreenEvents& other) const = default;

private:
    static uint32_t bit(ScreenEvent event){
        return uint32_t(1) << (uint32_t)event;
    }

    uint32_t m_bits = 0;
};


//  A fade is uniform over the middle 80% of the screen, the shared
//  Black/WhiteScreenWatcher's stddev test (upstream #1717): a dim cave floor
//  is textured, so it is no fade (BDSP's Oreburgh Gate, mean 46, lab-B
//  2026-11-08). Check against the Gate's frames on the console before
//  tightening.
constexpr double FADE_MAX_STDDEV_SUM = 10;
//  Dark goes to an rgb sum of 225, as a cave mouth's fade is dim, 45 to 64
//  a channel, not black.
constexpr double FADE_MAX_DARK_SUM = 225;
//  White from an rgb sum of 500, as the shared WhiteScreenWatcher's.
constexpr double FADE_MIN_WHITE_SUM = 500;
//  The fade `middle` (the stats of the screen's middle 80%) shows, if any.
WarpFade fade_of(const ImageStats& middle);
//  fade_of() for a screen.
WarpFade fade_of_screen(const ImageViewRGB32& screen);


//  A game's detectors, as events. Observers may keep state between frames
//  (the frame before, for SCREEN_STILL; when a banner was last read, to
//  gate OCR), so one observer watches one stream at a time.
class ScreenEventObserver{
public:
    virtual ~ScreenEventObserver() = default;

    //  What `frame` shows. `now` is the caller's clock, for observers that
    //  rate-limit expensive reads. A null frame shows nothing.
    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) = 0;

    //  The boxes the detectors read, for the video overlay.
    virtual void make_overlays(VideoOverlaySet& items) const = 0;
};


//  An observer as a run_until callback: it records every event it sees and
//  stops the action on any of `stop_on` (none: it only records).
class ScreenEventWatcher : public VisualInferenceCallback{
public:
    ScreenEventWatcher(ScreenEventObserver& observer, ScreenEvents stop_on = {});

    virtual void make_overlays(VideoOverlaySet& items) const override;
    using VisualInferenceCallback::process_frame;
    virtual bool process_frame(const VideoSnapshot& frame) override;

    //  Every event seen since construction (or the last reset()).
    ScreenEvents seen() const{
        return ScreenEvents::from_bits(m_seen.load(std::memory_order_acquire));
    }
    //  The events of the frame that stopped the action ("nothing" if none did).
    ScreenEvents stopped_on() const{
        return ScreenEvents::from_bits(m_stopped_on.load(std::memory_order_acquire));
    }
    void reset();

private:
    ScreenEventObserver& m_observer;
    const ScreenEvents m_stop_on;
    std::atomic<uint32_t> m_seen{0};
    std::atomic<uint32_t> m_stopped_on{0};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
