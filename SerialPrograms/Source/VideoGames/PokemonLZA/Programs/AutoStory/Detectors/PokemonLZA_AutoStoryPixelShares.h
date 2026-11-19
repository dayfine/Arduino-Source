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

//  Cyan glow: blue over red by 50, green over red by 30, blue over 120, blue >= green
//  - 20 (keeps out mint and sign green, which have green far over blue).
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  As cyan_share(), dimmer (red margins 40 and 25, blue over 70): the autosave glow
//  over a dark or lamp-lit scene.
double dim_cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The share of `box`'s pixels whose every channel is above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The controls hint's sky blue: blue over 200, green over 160, blue over red by 60.
double sky_blue_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The mission banner's yellow: red over 200, green over 140, blue under 130, red
//  over blue by 110.
double banner_yellow_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  The mission complete green: green over 180, over red by 40 and over blue by 70.
double complete_green_share(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  `box` moved `dx` pixels (of 1920) right and `dy` pixels (of 1080) down.
ImageFloatBox shifted(ImageFloatBox box, double dx, double dy = 0);


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
