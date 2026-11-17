/*  Screen Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What an autonomous program can see on the screen, in words every game
 *  shares: a fade, a location banner, a dialogue box, a battle starting.
 *  Each game implements ScreenEventObserver with its own detectors; the
 *  judgements built on these events are then the same for all of them.
 *
 *  An observer is asked frame by frame.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenEvents_ScreenEvents_H
#define PokemonAutomation_CommonTools_ScreenEvents_ScreenEvents_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
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


//  Whether `a` and `b` both have a frame of the same size within the
//  still-screen RMSD threshold (2.0): the screen barely changed
//  (ScreenEvent::SCREEN_STILL).
bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b);



}  // namespace PokemonAutomation
#endif
