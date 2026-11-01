#pragma once
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
namespace PokemonAutomation{ namespace NintendoSwitch{ namespace PokemonBDSP{
// AutoStory-specific transition handling; shared watchers retain upstream behavior.
//  A white screen (RGB sum over 500, stddev sum under 10) in `box`,
//  present or gone for `duration`.
class WhiteScreenWatcher : public DetectorToFinder<WhiteScreenDetector>{
public:
    WhiteScreenWatcher(Color color, const ImageFloatBox& box, FinderType finder_type, std::chrono::milliseconds duration)
        : DetectorToFinder("WhiteScreenWatcher", finder_type, duration, color, box, 500, 10)
    {}
};

//  The screen in `box` went white for 100 ms, then stopped being white for
//  100 ms: an encounter's flash.
class WhiteScreenOverWatcher : public VisualInferenceCallback{
public:
    WhiteScreenOverWatcher(Color color, const ImageFloatBox& box);

    bool white_is_over(const ImageViewRGB32& frame);

    virtual void make_overlays(VideoOverlaySet& items) const override;

    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;

private:
    WhiteScreenWatcher m_on;
    WhiteScreenWatcher m_off;
    bool m_has_been_white = false;
    std::atomic<bool> m_white_is_over = false;
};
}}}
