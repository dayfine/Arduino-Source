/*  Screen Event Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The screen events of a stream of frames, in order and timed, plus the
 *  run_until watcher that records them.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenEventLog_H
#define PokemonAutomation_Pokemon_ScreenEventLog_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"

namespace PokemonAutomation{


//  Whether an event started or stopped showing.
enum class ScreenEventEdge : uint8_t{
    ROSE,
    FELL,
};

//  One event starting or stopping to show, at the frame that showed it.
struct ScreenEventChange{
    WallClock time = WallClock::min();
    ScreenEvent event = ScreenEvent::FADE_BLACK;
    ScreenEventEdge edge = ScreenEventEdge::ROSE;

    bool operator==(const ScreenEventChange& other) const = default;
};

//  How many changes a log keeps.
constexpr size_t SCREEN_EVENT_LOG_CAPACITY = 256;

//  The events of a stream of frames, in order and timed: what rose and fell
//  when. It keeps the newest `capacity` changes and counts the ones it dropped.
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
    ScreenEvents seen() const;
    //  The events of the frame that stopped the action ("nothing" if none did).
    ScreenEvents stopped_on() const;
    //  A copy of the timed log of every frame since construction (or the
    //  last reset()).
    ScreenEventLog log() const;
    void reset();

private:
    ScreenEventObserver& m_observer;
    const ScreenEvents m_stop_on;
    mutable Mutex m_log_lock;
    ScreenEvents m_seen;
    ScreenEvents m_stopped_on;
    ScreenEventLog m_log;
};



}  // namespace PokemonAutomation
#endif
