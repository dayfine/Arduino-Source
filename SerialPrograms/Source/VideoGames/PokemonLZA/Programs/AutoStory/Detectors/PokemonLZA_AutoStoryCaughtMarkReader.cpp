/*  PokemonLZA AutoStory - Caught mark reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "PokemonLZA_AutoStoryCaughtMarkReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


namespace{

//  The name plate's HP bar: the widest flat green bar in the upper two
//  thirds of the screen.
std::optional<ImagePixelBox> find_plate_bar(const ImageViewRGB32& screen){
    const double scale = screen.height() / 1080.0;
    const ImageViewRGB32 upper = extract_box_reference(screen, ImageFloatBox(0, 0, 1, 0.68));
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(upper, 0, 125, 150, 255, 60, 175);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)(150 * scale * scale));
    std::optional<ImagePixelBox> best;
    WaterfillObject object;
    while (iter->find_next(object, false)){
        const size_t w = object.width() + 1;
        const size_t h = object.height() + 1;
        const bool flat = w >= 60 * scale && h <= 24 * scale && w >= 4 * h;
        if (!flat || object.area * 2 < w * h){
            continue;
        }
        if (!best || w > best->width()){
            best = ImagePixelBox(object.min_x, object.min_y, object.max_x + 1, object.max_y + 1);
        }
    }
    return best;
}

} // namespace


std::optional<bool> lock_target_caught(const ImageViewRGB32& screen){
    const std::optional<ImagePixelBox> bar = find_plate_bar(screen);
    if (!bar){
        return std::nullopt;
    }
    const double w = (double)bar->width();
    const size_t min_x = (size_t)(bar->min_x + 0.6 * w);
    const size_t max_x = std::min(screen.width(), (size_t)(bar->min_x + 1.5 * w));
    const size_t min_y = (size_t)std::max(0.0, bar->min_y - 0.85 * w);
    const size_t max_y = (size_t)std::max(0.0, bar->min_y - 0.45 * w);
    size_t red = 0;
    size_t white = 0;
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            const Color pixel(screen.pixel(x, y));
            const int r = pixel.red();
            red += r > 190 && r > pixel.green() + 70 && r > pixel.blue() + 50;
            white += r > 215 && pixel.green() > 215 && pixel.blue() > 215;
        }
    }
    //  37 red and 42 white at 1280x720 on the caught Mareep.
    const double area_scale = (screen.height() / 720.0) * (screen.height() / 720.0);
    return red >= 15 * area_scale && white >= 15 * area_scale;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
