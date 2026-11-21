/*  PokemonLZA AutoStory - Pixel shares
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryPixelShares.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 50 && pixel.green() > pixel.red() + 30 && pixel.blue() > 120;
    });
}

double dim_cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 40 && pixel.green() > pixel.red() + 25 && pixel.blue() > 70;
    });
}

//  The share of `box`'s pixels whose every channel is above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200;
    });
}

//  The share of `box`'s pixels in the controls hint card's sky blue (about
//  125, 210, 250): blue over 200, green over 160, blue over red by 60.
double sky_blue_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > 200 && pixel.green() > 160 && pixel.blue() > pixel.red() + 60;
    });
}

//  The share of `box`'s pixels in the mission banner's yellow (about 250,
//  210, 40 over a scene): red over 200, green over 140, blue under 130,
//  red over blue by 110.
double banner_yellow_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 140 && pixel.blue() < 130 &&
            pixel.red() > pixel.blue() + 110;
    });
}

//  The share of `box`'s pixels in the mission complete card's green (about
//  165, 240, 125): green over 180, over red by 40 and over blue by 70.
double complete_green_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.green() > 180 && pixel.green() > pixel.red() + 40 && pixel.green() > pixel.blue() + 70;
    });
}

ImageFloatBox shifted(ImageFloatBox box, double dx){
    box.x += dx / 1920;
    return box;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
