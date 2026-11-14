/*  PokemonLZA AutoStory - Mission complete detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryMissionCompleteDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On 1920x1080: the word's green fills x 615-1300,
//  y 271-349, none of it 20 px above or below; the bar's yellow fills
//  y 390-585, read left and right of the name as for the banner.
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

//  On the card the word is 70% green, nothing around it is green and both
//  bar boxes are 100% yellow. Other screens have the word 11% green at
//  most; the main mission banners, which have the same bar, 2% at most.
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
