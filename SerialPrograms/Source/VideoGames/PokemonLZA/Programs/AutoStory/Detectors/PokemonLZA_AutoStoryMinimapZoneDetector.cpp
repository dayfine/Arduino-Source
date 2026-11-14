/*  PokemonLZA AutoStory - Minimap zone detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryMinimapZoneDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The wild zone's green on the minimap (about 160, 240, 192, and 144,
//  224, 160 where darker): green over 150 and over red and blue by 20.
bool zone_green(Color pixel){
    return pixel.green() > 150 && pixel.green() > pixel.red() + 20 && pixel.green() > pixel.blue() + 20;
}

//  The player's arrow's blue (0, 160, 240 and 0, 144, 240; its view cone,
//  112, 208, 240, is left out by red under 80 and green under 200).
bool arrow_blue(Color pixel){
    return pixel.blue() > 200 && pixel.red() < 80 && pixel.green() > 100 && pixel.green() < 200;
}

} // namespace


MinimapZoneDetector::MinimapZoneDetector(Color color)
    : m_color(color)
    , m_middle(150.0 / 1920, 150.0 / 1080, 60.0 / 1920, 60.0 / 1080)
{}

void MinimapZoneDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_middle);
}

//  In Wild Zone 1 at 03.08's start the 60 x 60 middle is 46-47% zone green
//  and 10% arrow blue (CatchesToZone1Start/0075-0077, console B
//  2026-11-14). Of 82 frames in the city and its cutscenes none has over
//  0.1% of either, except one cutscene (ArrivalPastMissionBanner/0114)
//  with 29% of a scenery green and no arrow, and the ad's blue sky
//  (ArrivalToAd/0043) with 10% arrow blue and no green: both readings are
//  needed, green at a quarter and the arrow at 4%.
bool MinimapZoneDetector::detect(const ImageViewRGB32& screen){
    return share_of(screen, m_middle, zone_green) >= 0.25 && share_of(screen, m_middle, arrow_blue) >= 0.04;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
