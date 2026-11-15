/*  Covered Watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A watch for a screen that ends in frame time and reports whether its
 *  frames cover the window (Pokemon_FrameCoverage.h). run_until() and
 *  wait_until() end a watch in wall time and return -1 for "did not fire"
 *  whether the watcher inspected a frame every 33 ms or one a second, so a
 *  banner that showed between two inspected frames reads as "it did not
 *  show". These wrappers keep those routines and the watcher as
 *  they are, record the capture times of the frames the watcher was
 *  handed, keep watching past the window's end until a frame captured a
 *  tail after it has been inspected (bounded by a wall-clock cap), and
 *  return SEEN, NOT_SEEN or UNKNOWN.
 *
 *  The tail is the watcher's hold. A watcher that must see the screen for
 *  a hold (DetectorToFinder) fires a hold after the screen shows, so a
 *  screen that shows in the window's last hold fires only after the window
 *  ends. A fire in the tail still means the screen showed by the end.
 *
 */

#ifndef PokemonAutomation_Pokemon_CoveredWatch_H
#define PokemonAutomation_Pokemon_CoveredWatch_H

#include <functional>
#include <optional>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "Pokemon_FrameCoverage.h"

namespace PokemonAutomation{
namespace Pokemon{


//  `watcher` as it is, plus the frames it was handed. Frames captured
//  before `start` are not handed to `watcher`: they show the screen before
//  the window opened. It stops the session when `watcher` fires, or, once
//  the window's end is known, on the first frame captured at or after the
//  end plus `tail`. Call end_at() and read fired() and coverage() only
//  while no inference routine runs it.
class CoveredWatcher : public VisualInferenceCallback{
public:
    CoveredWatcher(
        VisualInferenceCallback& watcher, WallClock start, Milliseconds tail,
        std::optional<WallClock> end = std::nullopt
    );

    virtual void make_overlays(VideoOverlaySet& items) const override;
    using VisualInferenceCallback::process_frame;
    virtual bool process_frame(const VideoSnapshot& frame) override;

    //  The window ends at `end`: stop on the first frame captured at or
    //  after `end` plus the tail.
    void end_at(WallClock end){
        m_end = end;
    }
    bool fired() const{
        return m_fired;
    }
    const FrameCoverage& coverage() const{
        return m_coverage;
    }

private:
    VisualInferenceCallback& m_watcher;
    FrameCoverage m_coverage;
    Milliseconds m_tail;
    std::optional<WallClock> m_end;
    bool m_fired = false;
};


//  How a covered watch judges and bounds its window. The zero defaults are
//  the strictest: no gap is small enough for NOT_SEEN, no tail, no cap.
struct WatchLimits{
    //  judge_watch().
    Milliseconds max_gap = Milliseconds(0);
    //  The watcher's hold: how long past the window's end to keep watching.
    Milliseconds tail = Milliseconds(0);
    //  How much longer than the end plus the tail to wait in wall time for
    //  a frame captured that late.
    Milliseconds cap = Milliseconds(0);
    //  How often the watcher is handed the newest frame. At most `max_gap`,
    //  or frames cannot be inspected often enough to cover the window.
    Milliseconds period = Milliseconds(50);
};


//  Runs `command` with `watcher` live over the window from now to the
//  command's end. When the command ends without a fire, keeps watching
//  until a frame captured `limits.tail` after that end has been inspected,
//  at most `limits.cap` longer in wall time, so the window is judged on
//  frames up to its end and a screen that shows just as the command ends
//  is still caught.
template <typename ControllerContext>
WatchResult run_watched(
    VideoStream& stream, ControllerContext& context,
    std::function<void(ControllerContext& context)>&& command,
    const WatchLimits& limits, VisualInferenceCallback& watcher
);

//  Watches with `watcher` for `duration` in frame time: until it fires or a
//  frame captured `duration` plus `limits.tail` from now has been
//  inspected, at most `limits.cap` longer in wall time.
WatchResult watch_for(
    VideoStream& stream, CancellableScope& scope, Milliseconds duration,
    const WatchLimits& limits, VisualInferenceCallback& watcher
);

template <typename ControllerContext>
WatchResult run_watched(
    VideoStream& stream, ControllerContext& context,
    std::function<void(ControllerContext& context)>&& command,
    const WatchLimits& limits, VisualInferenceCallback& watcher
){
    CoveredWatcher covered(watcher, current_time(), limits.tail);
    run_until<ControllerContext>(stream, context, std::move(command), {covered}, limits.period);
    const WallClock end = current_time();
    if (!covered.fired()){
        covered.end_at(end);
        wait_until(stream, context, end + limits.tail + limits.cap, {covered}, limits.period);
    }
    return judge_watch(covered.fired(), covered.coverage(), end, limits.max_gap);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
