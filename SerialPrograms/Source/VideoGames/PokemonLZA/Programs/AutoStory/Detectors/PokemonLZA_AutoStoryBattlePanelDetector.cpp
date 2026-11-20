/*  PokemonLZA AutoStory - Battle panel detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryBattlePanelDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The shares of the badge's and the bar's pixels that make the panel.
constexpr double BADGE_SHARE = 0.5;
constexpr double BAR_SHARE = 0.8;

//  An HP bar pixel: its fill (red, yellow or green: channels 90 or more
//  apart), or its grey trough (channels within 25, mean 60 to 140).
bool hp_bar_pixel(Color color){
    const int r = color.red();
    const int g = color.green();
    const int b = color.blue();
    const int spread = std::max({r, g, b}) - std::min({r, g, b});
    const int mean = (r + g + b) / 3;
    return spread >= 90 || (spread <= 25 && mean >= 60 && mean <= 140);
}

} // namespace


BattlePanelDetector::BattlePanelDetector(Color color)
    : m_color(color)
    , m_badge(240.0 / 1920, 799.0 / 1080, 80.0 / 1920, 19.0 / 1080)
    , m_bar(236.0 / 1920, 882.0 / 1080, 204.0 / 1920, 20.0 / 1080)
{}

void BattlePanelDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_badge);
    items.add(m_color, m_bar);
}

bool BattlePanelDetector::detect(const ImageViewRGB32& screen){
    return sky_blue_share(screen, m_badge) >= BADGE_SHARE && share_of(screen, m_bar, hp_bar_pixel) >= BAR_SHARE;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
