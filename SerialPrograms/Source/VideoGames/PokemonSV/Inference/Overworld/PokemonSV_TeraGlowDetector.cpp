/*  Tera Glow Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <vector>
#include "Common/Cpp/Color.h"
#include "PokemonSV_TeraGlowDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


std::optional<TeraGlow> find_tera_glow(const ImageViewRGB32& screen){
    const size_t width = screen.width();
    const size_t height = screen.height();
    if (width == 0 || height == 0){
        return std::nullopt;
    }

    //  Sample on the grid of a 960x540 frame with a step of 2.
    const double step_x = width / 480.0;
    const double step_y = height / 270.0;
    auto at = [&](size_t gx, size_t gy){
        return Color(screen.pixel(
            std::min(width - 1, (size_t)(gx * step_x)),
            std::min(height - 1, (size_t)(gy * step_y))
        ));
    };

    std::vector<std::pair<size_t, size_t>> orb;
    for (size_t gy = 270 / 4; gy < 270 * 3 / 4; gy++){
        for (size_t gx = 0; gx < 480 * 82 / 100; gx++){
            const Color c = at(gx, gy);
            const int r = c.red(), g = c.green(), b = c.blue();
            if (b > 150 && b - g > 90 && r - g > 30){
                orb.emplace_back(gx, gy);
            }
        }
    }
    if (orb.empty()){
        return std::nullopt;
    }

    std::vector<size_t> xs, ys;
    for (const auto& p : orb){
        xs.push_back(p.first);
        ys.push_back(p.second);
    }
    std::nth_element(xs.begin(), xs.begin() + xs.size() / 2, xs.end());
    std::nth_element(ys.begin(), ys.begin() + ys.size() / 2, ys.end());
    const size_t cx = xs[xs.size() / 2];
    const size_t cy = ys[ys.size() / 2];

    size_t near = 0;
    for (const auto& p : orb){
        if ((p.first > cx ? p.first - cx : cx - p.first) < 15 && (p.second > cy ? p.second - cy : cy - p.second) < 15){
            near++;
        }
    }

    size_t halo = 0;
    for (size_t gy = cy > 20 ? cy - 20 : 0; gy < std::min<size_t>(270, cy + 20); gy++){
        for (size_t gx = cx > 25 ? cx - 25 : 0; gx < std::min<size_t>(480, cx + 25); gx++){
            const Color c = at(gx, gy);
            if (c.red() > 200 && c.green() > 150 && c.red() >= c.blue()){
                halo++;
            }
        }
    }

    if (near < 5 || halo < 30){
        return std::nullopt;
    }
    return TeraGlow{cx / 480.0, cy / 270.0, near, halo};
}


}
}
}
