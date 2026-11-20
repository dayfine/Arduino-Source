/*  PokemonLZA AutoStory - Mission banner detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryMissionBannerDetectors.h"
#include "PokemonLZA_AutoStoryPixelShares.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The mission banner's yellow: red over 200, green over 140, blue under 130, red
//  over blue by 110.
double banner_yellow_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 140 && pixel.blue() < 130 &&
            pixel.red() > pixel.blue() + 110;
    });
}

//  The mission complete green: green over 180, over red by 40 and over blue by 70.
double complete_green_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.green() > 180 && pixel.green() > pixel.red() + 40 && pixel.green() > pixel.blue() + 70;
    });
}

} // namespace


//  On 1080p: the bar's yellow ends x 430-600 and 1320-1500, y 425-585, past any name;
//  the icon's white at x 700-710 and 724-734, y 337-358, the "!" stroke at x 716-719.
MainMissionBannerDetector::MainMissionBannerDetector(Color color)
    : m_color(color)
    , m_bar_left(430.0 / 1920, 425.0 / 1080, 170.0 / 1920, 160.0 / 1080)
    , m_bar_right(1320.0 / 1920, 425.0 / 1080, 180.0 / 1920, 160.0 / 1080)
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

//  On the banner: bar boxes 100% yellow, strips around 0-1%, icon boxes 100%. Other
//  screens: bar 8% at most, the icon's weakest box 27% (a city flyover). The header's
//  length moves the icon (Italian: dx -96), so it is searched across x 400-960.
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
    for (double dx = -300; dx <= 226; dx += 2){
        if (share_of(screen, shifted(m_icon_left, dx), bright) >= 0.7 &&
            share_of(screen, shifted(m_icon_right, dx), bright) >= 0.7 &&
            share_of(screen, shifted(m_icon_stroke, dx), orange) >= 0.7
        ){
            return true;
        }
    }
    return false;
}


//  On 1080p: big green "COMPLETE" at x 615-1300, y 271-349, none of its green 20 px
//  above or below; the bar is read left and right of the name, as for the banner.
MissionCompleteDetector::MissionCompleteDetector(Color color)
    : m_color(color)
    , m_word(640.0 / 1920, 285.0 / 1080, 640.0 / 1920, 55.0 / 1080)
    , m_above(620.0 / 1920, 245.0 / 1080, 680.0 / 1920, 20.0 / 1080)
    , m_below(620.0 / 1920, 358.0 / 1080, 680.0 / 1920, 20.0 / 1080)
    , m_bar_left(430.0 / 1920, 440.0 / 1080, 390.0 / 1920, 140.0 / 1080)
    , m_bar_right(1100.0 / 1920, 440.0 / 1080, 400.0 / 1920, 140.0 / 1080)
{}

void MissionCompleteDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_word);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
    items.add(m_color, m_bar_left);
    items.add(m_color, m_bar_right);
}

//  On the card: word 70% green, nothing green around, bar boxes 100% yellow. Others:
//  word 11% at most; the mission banners (same bar) 2%.
bool MissionCompleteDetector::detect(const ImageViewRGB32& screen){
    if (complete_green_share(screen, m_word) < 0.40){
        return false;
    }
    if (complete_green_share(screen, m_above) > 0.10 || complete_green_share(screen, m_below) > 0.10){
        return false;
    }
    return banner_yellow_share(screen, m_bar_left) >= 0.6 && banner_yellow_share(screen, m_bar_right) >= 0.6;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
