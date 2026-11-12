/*  PokemonLZA AutoStory - Pixel shares
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How much of a box's pixels are of a color: what the badge detectors
 *  (Detectors/) read.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPixelShares_H
#define PokemonAutomation_PokemonLZA_AutoStoryPixelShares_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The share of `box`'s pixels that `matches` (a Color -> bool).
template <typename Matches>
double share_of(const ImageViewRGB32& screen, const ImageFloatBox& box, Matches&& matches);

//  The share of `box`'s pixels that glow cyan: blue over red by 50, green
//  over red by 30, blue over 120.
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The share of `box`'s pixels whose every channel is above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The share of `box`'s pixels in the controls hint card's sky blue (about
//  125, 210, 250): blue over 200, green over 160, blue over red by 60.
double sky_blue_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The share of `box`'s pixels in the mission banner's yellow (about 250,
//  210, 40 over a scene): red over 200, green over 140, blue under 130,
//  red over blue by 110.
double banner_yellow_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The share of `box`'s pixels in the mission complete card's green (about
//  165, 240, 125): green over 180, over red by 40 and over blue by 70.
double complete_green_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  `box` moved `dx` pixels (of 1920) right.
ImageFloatBox shifted(ImageFloatBox box, double dx);


template <typename Matches>
double share_of(const ImageViewRGB32& screen, const ImageFloatBox& box, Matches&& matches){
    const ImageViewRGB32 area = extract_box_reference(screen, box);
    const size_t total = area.width() * area.height();
    if (total == 0){
        return 0;
    }
    size_t count = 0;
    for (size_t y = 0; y < area.height(); y++){
        for (size_t x = 0; x < area.width(); x++){
            if (matches(Color(area.pixel(x, y)))){
                count++;
            }
        }
    }
    return (double)count / (double)total;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
