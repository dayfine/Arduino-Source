/*  PokemonLZA AutoStory - Controls hint detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryControlsHintDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


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
