/*  BDSP AutoStory - Location banner watcher
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading the location banner through an action, a few times per showing.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LocationBannerWatcher_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LocationBannerWatcher_H

#include <cstddef>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Reads the location banner at most twice per showing: when its box shows,
//  and once more 300 ms later in case the first frame caught it sliding in.
//  The box test runs on every frame; OCR (tens of ms) only then (#268's cost
//  rule: the old look read every poll while the box showed, 30 to 40 reads).
class LocationBannerReads{
public:
    //  The banner's text if this frame was read, "" if not.
    std::string read(const ImageViewRGB32& screen, WallClock now);

private:
    size_t m_reads = 0;
    WallClock m_first = WallClock::min();
};

//  Watches the location banner through an action. With `expected` (a map's
//  banner name) it stops the action when that banner reads; without, it
//  only keeps the first banner read.
class LocationBannerWatcher : public VisualInferenceCallback{
public:
    explicit LocationBannerWatcher(std::string expected = "");

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;

    //  The expected banner if it read, else the first banner read; "" if none.
    const std::string& seen() const{
        return m_seen;
    }
    //  The last banner read, for the log.
    const std::string& last_read() const{
        return m_last_read;
    }

private:
    std::string m_expected;
    LocationBannerReads m_reads;
    std::string m_seen;
    std::string m_last_read;
    bool m_matched = false;
};


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
