/*  PokemonLZA AutoStory - Dex registered detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryDexRegisteredDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


DexRegisteredDetector::DexRegisteredDetector(Color color)
    : m_color(color)
    , m_left_arc(852.0 / 1920, 140.0 / 1080, 20.0 / 1920, 80.0 / 1080)
    , m_right_arc(1100.0 / 1920, 140.0 / 1080, 20.0 / 1920, 80.0 / 1080)
    , m_tag(845.0 / 1920, 268.0 / 1080, 100.0 / 1920, 30.0 / 1080)
{}

void DexRegisteredDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_left_arc);
    items.add(m_color, m_right_arc);
    items.add(m_color, m_tag);
}

bool DexRegisteredDetector::detect(const ImageViewRGB32& screen){
    if (cyan_share(screen, m_left_arc) < 0.6 || cyan_share(screen, m_right_arc) < 0.4){
        return false;
    }
    //  The tag: slate (about 65, 100, 135) with white text on it.
    const double slate = share_of(screen, m_tag, [](Color pixel){
        const int r = pixel.red();
        return r >= 40 && r <= 100 && pixel.blue() >= 110 && pixel.blue() <= 175
            && pixel.blue() > r + 45 && pixel.green() > r + 20;
    });
    return slate >= 0.4 && white_share(screen, m_tag) >= 0.08;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
