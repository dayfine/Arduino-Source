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

ImageFloatBox shifted(ImageFloatBox box, double dx){
    box.x += dx / 1920;
    return box;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
