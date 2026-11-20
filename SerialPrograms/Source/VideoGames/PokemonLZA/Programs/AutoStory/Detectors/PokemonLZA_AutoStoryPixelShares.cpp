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
        return pixel.blue() > pixel.red() + 50 && pixel.green() > pixel.red() + 30 && pixel.blue() > 120 &&
            pixel.blue() + 20 >= pixel.green();
    });
}

double dim_cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 40 && pixel.green() > pixel.red() + 25 && pixel.blue() > 70 &&
            pixel.blue() + 20 >= pixel.green();
    });
}

double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200;
    });
}

ImageFloatBox shifted(ImageFloatBox box, double dx, double dy){
    box.x += dx / 1920;
    box.y += dy / 1080;
    return box;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
