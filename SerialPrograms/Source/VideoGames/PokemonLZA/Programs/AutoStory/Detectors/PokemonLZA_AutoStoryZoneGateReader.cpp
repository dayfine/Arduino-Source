/*  PokemonLZA AutoStory - Zone gate reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <vector>
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "PokemonLZA_AutoStoryZoneGateReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


namespace{

//  The pillars' lime, about (180, 250, 85) on console B, as ARGB bounds:
//  red 100-225, green 215-255, blue 0-135. The statues on top read as
//  their own blobs. The wall's stripes are this green too.
constexpr uint32_t LIME_MIN = 0xff64d700;
constexpr uint32_t LIME_MAX = 0xffe1ff87;

bool lime(uint32_t pixel){
    for (int shift = 0; shift < 24; shift += 8){
        const uint32_t channel = (pixel >> shift) & 0xff;
        if (channel < ((LIME_MIN >> shift) & 0xff) || channel > ((LIME_MAX >> shift) & 0xff)){
            return false;
        }
    }
    return true;
}

//  A pillar's share of its box that is lime: 0.59 and 0.64 on the gate
//  from mid-street; the wall's tallest blobs fill 0.31 to 0.53 of theirs.
constexpr double PILLAR_FILL = 0.55;

//  A solid lime blob at least twice as tall as wide (ten times at most),
//  at least 6% of the screen's height.
bool pillar_shaped(const WaterfillObject& blob, size_t screen_height){
    const double width = (double)blob.width();
    const double height = (double)blob.height();
    return height >= 0.06 * (double)screen_height
        && height >= 2 * width && height <= 10 * width
        && (double)blob.area >= PILLAR_FILL * width * height;
}

bool within_ratio(double a, double b, double ratio){
    return a <= b * ratio && b <= a * ratio;
}

//  The share of lime pixels between `left` and `right`, over the pillars'
//  height: the gate's opening shows the street through its barrier.
double lime_between(const ImageViewRGB32& screen, const WaterfillObject& left, const WaterfillObject& right){
    const size_t min_y = std::max(left.min_y, right.min_y);
    const size_t max_y = std::min(left.max_y, right.max_y);
    if (right.min_x <= left.max_x || max_y <= min_y){
        return 1;
    }
    size_t count = 0;
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = left.max_x; x < right.min_x; x++){
            count += lime(screen.pixel(x, y)) ? 1 : 0;
        }
    }
    return (double)count / (double)((right.min_x - left.max_x) * (max_y - min_y));
}

//  Whether `left` and `right` are a gate's two pillars: about the same
//  size, their feet on about the same line, 1.5 to 8 widths apart, and
//  less than 10% lime between them.
bool gate_pair(const ImageViewRGB32& screen, const WaterfillObject& left, const WaterfillObject& right){
    const double width = ((double)left.width() + (double)right.width()) / 2;
    const double height = ((double)left.height() + (double)right.height()) / 2;
    if (!within_ratio((double)left.width(), (double)right.width(), 1.4)
        || !within_ratio((double)left.height(), (double)right.height(), 1.33)
    ){
        return false;
    }
    if (std::abs((double)left.max_y - (double)right.max_y) > 0.25 * height){
        return false;
    }
    const double gap = (double)right.min_x - (double)left.max_x;
    if (gap < 1.5 * width || gap > 8 * width){
        return false;
    }
    return lime_between(screen, left, right) < 0.10;
}

} // namespace


std::optional<ZoneGateSighting> read_zone_gate(const ImageViewRGB32& screen){
    if (!screen || screen.width() < 16 || screen.height() < 16){
        return std::nullopt;
    }
    const double scale = (double)screen.height() / 1080.0;
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(screen, LIME_MIN, LIME_MAX);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)std::max(1.0, 300 * scale * scale));
    std::vector<WaterfillObject> pillars;
    WaterfillObject blob;
    while (iter->find_next(blob, false)){
        if (pillar_shaped(blob, screen.height())){
            pillars.push_back(blob);
        }
    }
    std::sort(pillars.begin(), pillars.end(), [](const WaterfillObject& a, const WaterfillObject& b){
        return a.min_x < b.min_x;
    });
    std::optional<ZoneGateSighting> best;
    size_t best_area = 0;
    for (size_t i = 0; i < pillars.size(); i++){
        for (size_t j = i + 1; j < pillars.size(); j++){
            const WaterfillObject& left = pillars[i];
            const WaterfillObject& right = pillars[j];
            if (left.area + right.area <= best_area || !gate_pair(screen, left, right)){
                continue;
            }
            best_area = left.area + right.area;
            best = ZoneGateSighting{
                .x = ((double)left.min_x + (double)right.max_x) / 2 / (double)screen.width(),
                .width = ((double)right.max_x - (double)left.min_x) / (double)screen.width(),
            };
        }
    }
    return best;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
