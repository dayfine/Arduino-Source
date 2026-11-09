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
//  The event of a fade: FADE_BLACK, FADE_WHITE, or none for WarpFade::NONE.
ScreenEvents fade_events(WarpFade fade);
//  The fade `events` show: BLACK before WHITE if they have both.
WarpFade fade_in(const ScreenEvents& events);
//  "dialog box, player can move"; "nothing" when empty.
std::string describe(const ScreenEvents& events);


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


//  Whether an event started or stopped showing.
enum class ScreenEventEdge : uint8_t{
    ROSE,
    FELL,
};

//  One event starting or stopping to show, at the frame that showed it.
struct ScreenEventChange{
    WallClock time;
    ScreenEvent event;
    ScreenEventEdge edge;

    bool operator==(const ScreenEventChange& other) const = default;
};

//  How many changes a log keeps. A walk's events change a few times a
//  second at most (a fade's black, a box, the screen going still), so this
//  is a minute or more of a busy scene.
constexpr size_t SCREEN_EVENT_LOG_CAPACITY = 256;

//  The events of a stream of frames, in order and timed: what rose and fell
//  when, not only what was seen (ScreenEventWatcher::seen()). The judges that
//  care about order read it (Pokemon_Interrupts.h): a flash then a battle is
//  an encounter, a box then a battle a trainer's sight.
//
//  It keeps the newest `capacity` changes and counts the ones it dropped.
//  Not thread-safe; ScreenEventWatcher guards its own.
class ScreenEventLog{
public:
    explicit ScreenEventLog(size_t capacity = SCREEN_EVENT_LOG_CAPACITY);

    //  Adds a frame: a change for each event that rose or fell since the
    //  frame before (the first frame's events all rise). `now` must not go
    //  back in time.
    void record(const ScreenEvents& frame, WallClock now);

    //  The changes kept, oldest first.
    const std::deque<ScreenEventChange>& changes() const{
        return m_changes;
    }
    //  How many changes were dropped, oldest first, to keep the capacity.
    size_t dropped() const{
        return m_dropped;
    }
    //  The events of the last frame recorded.
    ScreenEvents showing() const{
        return m_showing;
    }
    //  When the last frame was recorded; none before the first.
    std::optional<WallClock> last_frame() const{
        return m_last_frame;
    }
    void clear();

private:
    size_t m_capacity;
    std::deque<ScreenEventChange> m_changes;
    size_t m_dropped = 0;
    ScreenEvents m_showing;
    std::optional<WallClock> m_last_frame;
};
//  "+1200 ms dialog box rose", times from `origin`, one change per line.
std::string describe(const ScreenEventLog& log, WallClock origin);


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


//  An observer as a run_until callback: it records every event it sees, in
//  order in a log, and stops the action on any of `stop_on` (none: it only
//  records).
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
    //  A copy of the timed log of every frame since construction (or the
    //  last reset()).
    ScreenEventLog log() const;
    void reset();

private:
    ScreenEventObserver& m_observer;
    const ScreenEvents m_stop_on;
    std::atomic<uint32_t> m_seen{0};
    std::atomic<uint32_t> m_stopped_on{0};
    mutable Mutex m_log_lock;
    ScreenEventLog m_log;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
