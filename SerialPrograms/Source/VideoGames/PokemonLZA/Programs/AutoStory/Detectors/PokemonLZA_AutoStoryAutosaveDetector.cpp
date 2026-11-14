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

//  On the badge the icon is 19% white and 48% cyan, with nothing bright
//  left of it. A bright cutscene flash passes the icon's bounds too, but is
//  70%+ white and 39%+ cyan left of the icon as well.
//  Only the Chinese badge is checked; with a shorter text (English
//  "Saving...") a right-aligned badge moves the icon right, so the icon is
//  looked for from where it is in Chinese to 240 px right of it.
//  Glowing scenery (a lit blade, a bright sky) can match the icon somewhere
//  in that span, but glows on past it: the strips 14 px above and below the
//  icon are 22%+ white or cyan there and empty around the badge; they must
//  stay under 5%.
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
