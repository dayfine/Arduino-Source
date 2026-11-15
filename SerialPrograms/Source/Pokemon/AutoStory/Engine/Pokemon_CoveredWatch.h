/*  Covered Watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A watcher wrapper for a watch that ends in frame time and records
 *  whether its frames cover the window (Pokemon_FrameCoverage.h).
 *  run_until() and wait_until() end a watch in wall time and return -1 for
 *  "did not fire" whether the watcher inspected a frame every 33 ms or one
 *  a second, so a banner that showed between two inspected frames reads as
 *  "it did not show". CoveredWatcher keeps the watcher as it is, records
 *  the capture times of the frames the watcher was handed, and keeps
 *  watching past the window's end until a frame captured a tail after it
 *  has been inspected; judge_watch() then reads SEEN, NOT_SEEN or UNKNOWN.
 *
 *  The tail is the watcher's hold. A watcher that must see the screen for
 *  a hold (DetectorToFinder) fires a hold after the screen shows, so a
 *  screen that shows in the window's last hold fires only after the window
 *  ends. A fire in the tail still means the screen showed by the end.
 *
 */

#ifndef PokemonAutomation_Pokemon_CoveredWatch_H
#define PokemonAutomation_Pokemon_CoveredWatch_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
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



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
