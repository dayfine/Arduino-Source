/*  PokemonLZA AutoStory - Badge detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryBadgeDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The share of `box`'s pixels that glow cyan: blue over red by 50, green
//  over red by 30, blue over 120.
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    const ImageViewRGB32 area = extract_box_reference(screen, box);
    const size_t total = area.width() * area.height();
    if (total == 0){
        return 0;
    }
    size_t cyan = 0;
    for (size_t y = 0; y < area.height(); y++){
        for (size_t x = 0; x < area.width(); x++){
            const Color pixel(area.pixel(x, y));
            if (pixel.blue() > pixel.red() + 50 && pixel.green() > pixel.red() + 30 && pixel.blue() > 120){
                cyan++;
            }
        }
    }
    return (double)cyan / (double)total;
}

//  The share of `box`'s pixels whose every channel is above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    const ImageViewRGB32 area = extract_box_reference(screen, box);
    const size_t total = area.width() * area.height();
    if (total == 0){
        return 0;
    }
    size_t white = 0;
    for (size_t y = 0; y < area.height(); y++){
        for (size_t x = 0; x < area.width(); x++){
            const Color pixel(area.pixel(x, y));
            if (pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200){
                white++;
            }
        }
    }
    return (double)white / (double)total;
}


//  The icon and the strip left of it, `dx` pixels (of 1920) right of where
//  the Chinese badge has them.
bool autosave_icon_at(const ImageViewRGB32& screen, const ImageFloatBox& icon, const ImageFloatBox& left, double dx){
    ImageFloatBox icon_here = icon;
    ImageFloatBox left_here = left;
    icon_here.x += dx / 1920;
    left_here.x += dx / 1920;
    const double white = white_share(screen, icon_here);
    if (white < 0.08 || white > 0.40 || cyan_share(screen, icon_here) < 0.25){
        return false;
    }
    return white_share(screen, left_here) < 0.05 && cyan_share(screen, left_here) < 0.05;
}

} // namespace


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



AutosaveDetector::AutosaveDetector(Color color)
    : m_color(color)
    , m_icon(1603.0 / 1920, 73.0 / 1080, 27.0 / 1920, 31.0 / 1080)
    , m_left(1570.0 / 1920, 73.0 / 1080, 28.0 / 1920, 31.0 / 1080)
{}

void AutosaveDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_icon);
    items.add(m_color, m_left);
}

//  Measured on the user's run (console B, 2026-11-10, frames 1.5 s apart):
//  the badge's one frame (rec1_u132) has the icon 19% white and 48% cyan,
//  nothing left of it. The only other frames over the icon bounds (u103,
//  u104: a bright cutscene flash) are 70%+ white and 39%+ cyan left of it
//  too; none of the opening's 56 lab frames come near.
//  The badge is seen in Chinese only; with a shorter text (English
//  "Saving...") a right-aligned badge moves the icon right, so the icon is
//  looked for from where it is in Chinese to 240 px right of it.
bool AutosaveDetector::detect(const ImageViewRGB32& screen){
    for (double dx = 0; dx <= 240; dx += 4){
        if (autosave_icon_at(screen, m_icon, m_left, dx)){
            return true;
        }
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
