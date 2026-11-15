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


//  The badge on 1920x1080: the (+) icon, a white disk with a dark plus,
//  at x 1564-1586, y 128-150 (its box 8 px wider all round); the text
//  x 1630-1875, y 122-158; the strip under the text x 1640-1880, y 161-169.
SkipPromptDetector::SkipPromptDetector(Color color)
    : m_color(color)
    , m_icon(color, ButtonType::ButtonPlus, ImageFloatBox(1556.0 / 1920, 120.0 / 1080, 38.0 / 1920, 38.0 / 1080))
    , m_text(1630.0 / 1920, 122.0 / 1080, 245.0 / 1920, 36.0 / 1080)
    , m_strip(1640.0 / 1920, 161.0 / 1080, 240.0 / 1920, 8.0 / 1080)
{}

void SkipPromptDetector::make_overlays(VideoOverlaySet& items) const{
    m_icon.make_overlays(items);
    items.add(m_color, m_text);
    items.add(m_color, m_strip);
}

//  On the badge the text is 24% white and the strip is dark and even
//  (average 44, deviation 2). The text and strip tests stay beside the
//  icon's template match: the opening's white flash has the strip at 220.
bool SkipPromptDetector::detect(const ImageViewRGB32& screen){
    if (!m_icon.detect(screen)){
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
