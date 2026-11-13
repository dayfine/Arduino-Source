/*  PokemonLZA AutoStory - Autosave detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <initializer_list>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryAutosaveDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The icon (white, with a cyan glow, dim or bright), and the strips around
//  it (left, above, below) clear of white and bright cyan, `dx` pixels (of
//  1920) right of where the Chinese badge has them.
bool autosave_icon_at(
    const ImageViewRGB32& screen, const ImageFloatBox& icon,
    std::initializer_list<ImageFloatBox> around, double dx
){
    const ImageFloatBox icon_here = shifted(icon, dx);
    const double white = white_share(screen, icon_here);
    if (white < 0.08 || white > 0.40 || dim_cyan_share(screen, icon_here) < 0.25){
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
//  The icon's glow is dim over a dark or lamp-lit scene (about 28, 70, 85
//  on 02.04's check-in wall, console B 2026-11-13: frame 0075 of the 13:46
//  run, in Japanese, icon at dx 36), so the icon's glow is dim_cyan_share():
//  of the 3569 PLZA captures on console B to 2026-11-13 it finds 11 more
//  badges, every one a real badge, and no new false one.
bool AutosaveDetector::detect(const ImageViewRGB32& screen){
    for (double dx = 0; dx <= 240; dx += 4){
        if (autosave_icon_at(screen, m_icon, {m_left, m_above, m_below}, dx)){
            return true;
        }
    }
    return false;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
