/*  PokemonLZA AutoStory - Badge detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <initializer_list>
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryBadgeDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The share of `box`'s pixels that `matches`.
template <typename Matches>
double share_of(const ImageViewRGB32& screen, const ImageFloatBox& box, Matches&& matches){
    const ImageViewRGB32 area = extract_box_reference(screen, box);
    const size_t total = area.width() * area.height();
    if (total == 0){
        return 0;
    }
    size_t count = 0;
    for (size_t y = 0; y < area.height(); y++){
        for (size_t x = 0; x < area.width(); x++){
            if (matches(Color(area.pixel(x, y)))){
                count++;
            }
        }
    }
    return (double)count / (double)total;
}

//  The share of `box`'s pixels that glow cyan: blue over red by 50, green
//  over red by 30, blue over 120.
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 50 && pixel.green() > pixel.red() + 30 && pixel.blue() > 120;
    });
}

//  The share of `box`'s pixels whose every channel is above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200;
    });
}

//  The share of `box`'s pixels in the controls hint card's sky blue (about
//  125, 210, 250): blue over 200, green over 160, blue over red by 60.
double sky_blue_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > 200 && pixel.green() > 160 && pixel.blue() > pixel.red() + 60;
    });
}

ImageFloatBox shifted(ImageFloatBox box, double dx){
    box.x += dx / 1920;
    return box;
}


//  The icon, and the strips around it (left, above, below) clear of white
//  and cyan, `dx` pixels (of 1920) right of where the Chinese badge has
//  them.
bool autosave_icon_at(
    const ImageViewRGB32& screen, const ImageFloatBox& icon,
    std::initializer_list<ImageFloatBox> around, double dx
){
    const ImageFloatBox icon_here = shifted(icon, dx);
    const double white = white_share(screen, icon_here);
    if (white < 0.08 || white > 0.40 || cyan_share(screen, icon_here) < 0.25){
        return false;
    }
    for (const ImageFloatBox& box : around){
        const ImageFloatBox here = shifted(box, dx);
        if (white_share(screen, here) >= 0.05 || cyan_share(screen, here) >= 0.05){
            return false;
        }
    }
    return true;
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
    , m_above(1603.0 / 1920, 56.0 / 1080, 27.0 / 1920, 14.0 / 1080)
    , m_below(1603.0 / 1920, 108.0 / 1080, 27.0 / 1920, 14.0 / 1080)
{}

void AutosaveDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_icon);
    items.add(m_color, m_left);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
}

//  Measured on the user's run (console B, 2026-11-10, frames 1.5 s apart):
//  the badge's one frame (rec1_u132) has the icon 19% white and 48% cyan,
//  nothing left of it. The only other frames over the icon bounds (u103,
//  u104: a bright cutscene flash) are 70%+ white and 39%+ cyan left of it
//  too; none of the opening's 56 lab frames come near.
//  The badge is seen in Chinese only; with a shorter text (English
//  "Saving...") a right-aligned badge moves the icon right, so the icon is
//  looked for from where it is in Chinese to 240 px right of it.
//  The 01.01 run (console B, 2026-11-10 13:00) took two flyover frames for
//  the badge: 0033, a glowing blade at dx 40, and 0038, the sky at dx
//  152-172. Both glow on past the icon: the strips 14 px above and below it
//  are 22%+ white or cyan there, 0% on u132; they must stay under 5%.
bool AutosaveDetector::detect(const ImageViewRGB32& screen){
    for (double dx = 0; dx <= 240; dx += 4){
        if (autosave_icon_at(screen, m_icon, {m_left, m_above, m_below}, dx)){
            return true;
        }
    }
    return false;
}



//  Measured on 1920x1080 (console B, 2026-11-10): the card at x 946-971,
//  y 868-901 with its spine at x 974-976; the "?" at x 953-965, y 876-894.
ControlsHintDetector::ControlsHintDetector(Color color)
    : m_color(color)
    , m_card(947.0 / 1920, 872.0 / 1080, 24.0 / 1920, 28.0 / 1080)
    , m_glyph(955.0 / 1920, 879.0 / 1080, 8.0 / 1920, 7.0 / 1080)
    , m_around{
        {928.0 / 1920, 872.0 / 1080, 14.0 / 1920, 28.0 / 1080},
        {980.0 / 1920, 872.0 / 1080, 14.0 / 1920, 28.0 / 1080},
        {945.0 / 1920, 856.0 / 1080, 30.0 / 1920, 8.0 / 1080},
        {945.0 / 1920, 906.0 / 1080, 30.0 / 1920, 8.0 / 1080},
    }
{}

void ControlsHintDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_card);
    for (const ImageFloatBox& box : m_around){
        items.add(m_color, box);
    }
}

//  The hint's 13 frames (01.01 run 0001-0006, the user's u132-u138): the
//  card 88%+ sky blue, the "?" at most 48%, nothing around it. Of the 376
//  frames without it, the closest are the 01.01 run's 0056 (the sky
//  behind the girl: card 76%, "?" 100%, around 54%), 0060 (card 35%,
//  around 27%) and the user's u126 (card 22%, around 32%).
bool ControlsHintDetector::detect(const ImageViewRGB32& screen){
    if (sky_blue_share(screen, m_card) < 0.80 || sky_blue_share(screen, m_glyph) > 0.70){
        return false;
    }
    for (const ImageFloatBox& box : m_around){
        if (sky_blue_share(screen, box) > 0.10){
            return false;
        }
    }
    return true;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
