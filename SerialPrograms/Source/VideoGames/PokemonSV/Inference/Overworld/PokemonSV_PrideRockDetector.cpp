/*  Pride Rock Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "PokemonSV_PrideRockDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


namespace{

struct ManeSamples{
    size_t count = 0;
    double sum_x = 0;
};

ManeSamples red_mane_samples(const ImageViewRGB32& image, bool photo_mode){
    const size_t width = image.width();
    const size_t height = image.height();
    ManeSamples samples;
    for (size_t y = height / 8; y < height * 7 / 8; y += 4){
        for (size_t x = 0; x < width; x += 4){
            if (photo_mode){
                if (x < width * 2 / 10 || y > height * 7 / 10){
                    continue;
                }
            }else{
                if (x > width * 8 / 10 && y > height * 6 / 10){
                    continue;
                }
                //  The backpack alone: a wider box hid Pyroar's mane as the camera
                //  turned it to the middle (console 2026-10-26).
                if (x > width * 45 / 100 && x < width * 55 / 100 && y > height * 55 / 100 && y < height * 85 / 100){
                    continue;
                }
            }
            const Color pixel(image.pixel(x, y));
            if (pixel.red() > 170 && pixel.green() < 70 && pixel.blue() < 80){
                samples.count++;
                samples.sum_x += (double)x / width;
            }
        }
    }
    return samples;
}

}


std::optional<double> find_red_mane_x(const ImageViewRGB32& image, bool photo_mode){
    const ManeSamples samples = red_mane_samples(image, photo_mode);
    if (samples.count < 30){
        return std::nullopt;
    }
    return samples.sum_x / samples.count;
}

bool off_pride_rock(const ImageViewRGB32& image){
    //  The ground to either side of the player's feet. Yellow or green grass by
    //  day; at night the grass is dim but still greener than the blue-grey rock.
    //  The rock by day is brown, with little more green than blue.
    const size_t width = image.width();
    const size_t height = image.height();
    size_t total = 0;
    size_t grass = 0;
    for (size_t y = height * 84 / 100; y < height * 97 / 100; y += 2){
        for (size_t x = width * 30 / 100; x < width * 70 / 100; x += 2){
            if (x > width * 44 / 100 && x < width * 56 / 100){
                continue;
            }
            const Color pixel(image.pixel(x, y));
            const int r = pixel.red();
            const int g = pixel.green();
            const int b = pixel.blue();
            total++;
            if ((g > 0 && g - b > 0.35 * g) || (g >= r + 6 && g >= b + 12)){
                grass++;
            }
        }
    }
    const double grass_fraction = total == 0 ? 0 : (double)grass / total;
    //  On the rock at its edge, grass beyond the edge reads up to 0.48; off the
    //  rock, beside it in tall grass, 0.30 with no Pyroar in view.
    if (grass_fraction >= 0.6){
        return true;
    }
    return grass_fraction >= 0.25 && red_mane_samples(image, false).count < 30;
}


}
}
}
