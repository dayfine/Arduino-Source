/*  PokemonLZA AutoStory - Battle markers detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryBattleMarkersDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr double SLOT_CENTRES_X[] = {405, 432, 459, 486, 513};
constexpr double SLOT_WIDTH = 16;
constexpr double SLOT_HEIGHT = 12;
constexpr double SLOT_TOP = 800;

constexpr double TRIANGLE_SHARE = 0.2;
constexpr double NO_TRIANGLE_SHARE = 0.05;
constexpr double DOT_SHARE_MIN = 0.06;
constexpr double DOT_SHARE_MAX = 0.5;
constexpr size_t DOTS_MIN = 3;
constexpr int DOT_CHANNEL_SPREAD_MAX = 30;

//  The triangle's blue: red 70 or less, green 150 to 210, blue 225 or more.
double triangle_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() <= 70 && pixel.green() >= 150 && pixel.green() <= 210 && pixel.blue() >= 225;
    });
}

//  A dot's white: every channel over 200, within 30 of each other. Light paving stone
//  is as bright but yellow (channels 38-45 apart).
double dot_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        const int low = std::min({pixel.red(), pixel.green(), pixel.blue()});
        const int high = std::max({pixel.red(), pixel.green(), pixel.blue()});
        return low > 200 && high - low <= DOT_CHANNEL_SPREAD_MAX;
    });
}

} // namespace


BattleMarkersDetector::BattleMarkersDetector(Color color)
    : m_color(color)
{
    for (double centre : SLOT_CENTRES_X){
        m_slots.emplace_back(
            (centre - SLOT_WIDTH / 2) / 1920, SLOT_TOP / 1080, SLOT_WIDTH / 1920, SLOT_HEIGHT / 1080
        );
    }
}

void BattleMarkersDetector::make_overlays(VideoOverlaySet& items) const{
    for (const ImageFloatBox& slot : m_slots){
        items.add(m_color, slot);
    }
}

bool BattleMarkersDetector::detect(const ImageViewRGB32& screen){
    size_t triangles = 0;
    size_t dots = 0;
    for (const ImageFloatBox& slot : m_slots){
        const double blue = triangle_share(screen, slot);
        if (blue >= TRIANGLE_SHARE){
            triangles++;
            continue;
        }
        const double white = dot_share(screen, slot);
        if (blue < NO_TRIANGLE_SHARE && white >= DOT_SHARE_MIN && white <= DOT_SHARE_MAX){
            dots++;
        }
    }
    return triangles == 1 && dots >= DOTS_MIN;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
