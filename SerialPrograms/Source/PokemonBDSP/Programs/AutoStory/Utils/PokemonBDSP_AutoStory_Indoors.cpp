/*  BDSP AutoStory - Indoors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "PokemonBDSP_AutoStory_Indoors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Near-black pixels in one edge strip of the screen, out of all its pixels.
struct EdgeDark{
    size_t dark = 0;
    size_t count = 0;
};
void add_pixel(EdgeDark& edge, bool dark){
    edge.dark += dark;
    edge.count++;
}
bool mostly_dark(const EdgeDark& edge){
    return edge.count > 0 && edge.dark * 100 >= edge.count * 15;
}

}

bool indoors(const ImageViewRGB32& screen){
    const size_t width = screen.width(), height = screen.height();
    const size_t strip_h = height * 12 / 100, strip_w = width * 12 / 100;
    size_t warm = 0;
    size_t count = 0;
    EdgeDark top, bottom, left, right;
    for (size_t y = 0; y < height; y += 8){
        for (size_t x = 0; x < width; x += 8){
            const Color pixel(screen.pixel(x, y));
            count++;
            warm += pixel.red() > 150 && pixel.red() > pixel.blue() + 70 && pixel.green() > pixel.blue() + 20;
            const bool dark = pixel.red() + pixel.green() + pixel.blue() < 60;
            if (y < strip_h){
                add_pixel(top, dark);
            }
            if (y >= height - strip_h){
                add_pixel(bottom, dark);
            }
            if (x < strip_w){
                add_pixel(left, dark);
            }
            if (x >= width - strip_w){
                add_pixel(right, dark);
            }
        }
    }
    if (count == 0 || warm * 4 <= count){
        return false;
    }
    return mostly_dark(top) || mostly_dark(bottom) || mostly_dark(left) || mostly_dark(right);
}



}
}
}
