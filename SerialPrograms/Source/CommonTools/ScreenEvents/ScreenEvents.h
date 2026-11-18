/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What an autonomous program can see on the screen, in words every game
 *  shares: a fade, a location banner, a dialogue box, a battle starting.
 *  Each game implements ScreenEventObserver with its own detectors; the
 *  judgements built on these events are then the same for all of them.
 *
 *  An observer runs during a controller action as a ScreenEventWatcher
 *  (a VisualInferenceCallback for run_until), or is asked frame by frame.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenEvents_ScreenEvents_H
#define PokemonAutomation_CommonTools_ScreenEvents_ScreenEvents_H

#include <atomic>
#include <cstdint>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "ScreenEventLog.h"
#include "ScreenEventTypes.h"

namespace PokemonAutomation{


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



}  // namespace PokemonAutomation
#endif
