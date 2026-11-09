/*  BDSP AutoStory - Location
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the game says the player is: the location banner shown on entering
 *  a map, and the map named on the save screen's card.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Location_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Location_H

#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Lower-case letters and digits only: "Route 202" and "ROUTE 2O2 " compare
//  by "route202".
std::string normalize_location_name(const std::string& text);

//  The game's language, for the reads that still use text (item names, the
//  location banner, a sign's or talk's first box), set once from the
//  program's options (StoryRunOptions) before the story runs; English until
//  then. story_reads_text() says whether those reads have a model and
//  dictionary for it (English only, so far); when not, callers use
//  language-free checks: the banner's box showing, a sign's box opening, the
//  first row of a Bag pocket.
Language story_language();
//  Sets the active story language.
void set_story_language(Language language);
//  Returns whether text recognition is supported for the current story language.
bool story_reads_text();
//  Returns whether text recognition is supported for the specified language.
bool story_reads_text(Language language);

//  What the reads below return for a banner or save card that showed, when
//  there is no text reader for the game's language (story_reads_text()):
//  location_banner_matches() takes it as a match.
inline const std::string UNREAD_LOCATION = "(unread)";

//  The location banner (the white box at the top left that names a map for a
//  few seconds after the player enters it), read by OCR and normalized;
//  empty when no banner shows.
std::string read_location_banner(const ImageViewRGB32& screen);
//  Whether the location banner's white box shows, without reading it.
bool location_banner_showing(const ImageViewRGB32& screen);
//  The banner box's mean and spread, for the log.
std::string location_banner_stats(const ImageViewRGB32& screen);

//  The map named at the top left of the save screen's card ("Sandgem Town",
//  "Route 202"), read by OCR and normalized; empty if nothing reads.
std::string read_save_screen_location(const ImageViewRGB32& screen);

//  Whether the location banner's OCR `read` names the map `expected`. Both
//  are compared as lower-case letters and digits. OCR drops or swaps letters
//  ("andgemltown" for Sandgem Town on r109), so up to a quarter of the name's
//  letters may differ; the digits must match exactly, so Route 201 is never
//  read as Route 202.
bool location_banner_matches(const std::string& read, const std::string& expected);

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
