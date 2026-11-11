/*  PokemonLZA AutoStory - Main mission banner detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryMainMissionBannerDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Measured on 1920x1080 (console B, 2026-11-10, 01.01 run 3's 0181): the
//  bar's yellow fills x 430-820 and x 1100-1500 at y 425-585 (left and
//  right of the name), none of it 15 px above or below the bar; the icon's
//  hexagon is white at x 700-710 and 724-734, y 337-358, with the "!"'s
//  orange stroke at x 716-719 between them.
MainMissionBannerDetector::MainMissionBannerDetector(Color color)
    : m_color(color)
    , m_bar_left(430.0 / 1920, 425.0 / 1080, 390.0 / 1920, 160.0 / 1080)
    , m_bar_right(1100.0 / 1920, 425.0 / 1080, 400.0 / 1920, 160.0 / 1080)
    , m_above(430.0 / 1920, 375.0 / 1080, 1070.0 / 1920, 25.0 / 1080)
    , m_below(430.0 / 1920, 615.0 / 1080, 1070.0 / 1920, 25.0 / 1080)
    , m_icon_left(700.0 / 1920, 337.0 / 1080, 10.0 / 1920, 21.0 / 1080)
    , m_icon_right(724.0 / 1920, 337.0 / 1080, 10.0 / 1920, 21.0 / 1080)
    , m_icon_stroke(716.0 / 1920, 337.0 / 1080, 3.0 / 1920, 11.0 / 1080)
{}

void MainMissionBannerDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_bar_left);
    items.add(m_color, m_bar_right);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
    items.add(m_color, m_icon_left);
    items.add(m_color, m_icon_right);
    items.add(m_color, m_icon_stroke);
}

//  Measured on 01.01's runs 1-3, the lab's and the user's opening frames
//  and the other PLZA lab captures (746 frames, console B, 2026-11-10):
//  the banner's one frame (run 3's 0181) has both bar boxes 100% yellow,
//  the strips around it 0-1%, and the icon's three boxes 100% at dx 0. The
//  closest others: the bar boxes 8% at most, the icon's weakest box 27% at
//  most (run 2's 0032, the flyover). The icon is looked for 60 px either side, as a longer
//  or shorter header moves it.
bool MainMissionBannerDetector::detect(const ImageViewRGB32& screen){
    if (banner_yellow_share(screen, m_bar_left) < 0.6 || banner_yellow_share(screen, m_bar_right) < 0.6){
        return false;
    }
    if (banner_yellow_share(screen, m_above) > 0.3 || banner_yellow_share(screen, m_below) > 0.3){
        return false;
    }
    const auto bright = [](Color pixel){
        return pixel.red() > 225 && pixel.green() > 225 && pixel.blue() > 225;
    };
    const auto orange = [](Color pixel){
        return pixel.red() > 200 && pixel.blue() < 150;
    };
    for (double dx = -60; dx <= 60; dx += 2){
        if (share_of(screen, shifted(m_icon_left, dx), bright) >= 0.7 &&
            share_of(screen, shifted(m_icon_right, dx), bright) >= 0.7 &&
            share_of(screen, shifted(m_icon_stroke, dx), orange) >= 0.7
        ){
            return true;
        }
    }
    return false;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
