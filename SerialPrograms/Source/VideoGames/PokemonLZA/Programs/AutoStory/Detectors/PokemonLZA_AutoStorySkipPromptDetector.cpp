/*  PokemonLZA AutoStory - Skip prompt detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStorySkipPromptDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The badge on 1920x1080 (frame 0050 of the opening): the white (+) icon
//  at x 1564-1586, y 128-150; the text x 1630-1875, y 122-158; the strip
//  under the text x 1640-1880, y 161-169.
SkipPromptDetector::SkipPromptDetector(Color color)
    : m_color(color)
    , m_icon(1564.0 / 1920, 128.0 / 1080, 22.0 / 1920, 22.0 / 1080)
    , m_text(1630.0 / 1920, 122.0 / 1080, 245.0 / 1920, 36.0 / 1080)
    , m_strip(1640.0 / 1920, 161.0 / 1080, 240.0 / 1920, 8.0 / 1080)
{}

void SkipPromptDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_icon);
    items.add(m_color, m_text);
    items.add(m_color, m_strip);
}

//  On frame 0050 the icon is 70% white, the text 24%, and the strip is
//  dark and even (average 44, deviation 2). No other opening frame has the
//  icon over 40% white with a dark strip: the brightest (0044, a white
//  flash) has the icon at 38% and the strip at 220.
bool SkipPromptDetector::detect(const ImageViewRGB32& screen){
    if (white_share(screen, m_icon) < 0.40){
        return false;
    }
    const double text = white_share(screen, m_text);
    if (text < 0.10 || text > 0.45){
        return false;
    }
    const ImageStats strip = image_stats(extract_box_reference(screen, m_strip));
    return strip.average.sum() < 3 * 100.0 && strip.stddev.sum() < 3 * 15.0;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
