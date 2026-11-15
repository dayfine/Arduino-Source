/*  Covered Watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A watch for a screen that ends in frame time and reports whether its
 *  frames cover the window (Pokemon_FrameCoverage.h). run_until() and
 *  wait_until() end a watch in wall time and return -1 for "did not fire"
 *  whether the watcher inspected a frame every 33 ms or one a second: the
 *  PLZA 03.08 catch (2026-11-14) read a missed "Registered" banner as "no
 *  new species". These wrappers keep those routines and the watcher as
 *  they are, record the capture times of the frames the watcher was
 *  handed, keep watching until a frame captured at the window's end has
 *  been inspected (bounded by a wall-clock cap), and return SEEN, NOT_SEEN
 *  or UNKNOWN.
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


//  `watcher` as it is, plus the frames it was handed. It stops the session
//  when `watcher` fires, or once stop_at() is set, on the first frame
//  captured at or after it. Read fired() and coverage() only after the
//  inference routine it ran in has returned.
class CoveredWatcher : public VisualInferenceCallback{
public:
    CoveredWatcher(VisualInferenceCallback& watcher, WallClock start);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    using VisualInferenceCallback::process_frame;
    virtual bool process_frame(const VideoSnapshot& frame) override;

    //  Also stop on the first frame captured at or after `deadline`.
    void stop_at(WallClock deadline){
        m_stop_at = deadline;
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
    std::optional<WallClock> m_stop_at;
    bool m_fired = false;
};


//  Runs `command` with `watcher` live over the window from now to the
//  command's end. When the command ends without a fire, keeps watching until
//  a frame captured at or after that end has been inspected, at most `cap`
//  longer, so the window is judged on frames up to its end and a screen that
//  shows just as the command ends is still caught. `max_gap`: judge_watch().
template <typename ControllerContext>
WatchResult run_watched(
    VideoStream& stream, ControllerContext& context,
    std::function<void(ControllerContext& context)>&& command,
    Milliseconds max_gap, Milliseconds cap, VisualInferenceCallback& watcher
);

//  Watches with `watcher` for `duration` in frame time: until it fires or a
//  frame captured `duration` from now has been inspected, at most `cap`
//  longer than `duration` in wall time. `max_gap`: judge_watch().
WatchResult watch_for(
    VideoStream& stream, CancellableScope& scope, Milliseconds duration,
    Milliseconds max_gap, Milliseconds cap, VisualInferenceCallback& watcher
);

template <typename ControllerContext>
WatchResult run_watched(
    VideoStream& stream, ControllerContext& context,
    std::function<void(ControllerContext& context)>&& command,
    Milliseconds max_gap, Milliseconds cap, VisualInferenceCallback& watcher
){
    CoveredWatcher covered(watcher, current_time());
    run_until<ControllerContext>(stream, context, std::move(command), {covered});
    const WallClock end = current_time();
    if (!covered.fired() && !covered.coverage().reached(end)){
        covered.stop_at(end);
        wait_until(stream, context, cap, {covered});
    }
    return judge_watch(covered.fired(), covered.coverage(), end, max_gap);
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
