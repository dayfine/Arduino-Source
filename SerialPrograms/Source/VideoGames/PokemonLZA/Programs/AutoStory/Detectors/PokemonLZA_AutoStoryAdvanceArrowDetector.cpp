/*  PokemonLZA AutoStory - Advance arrow detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <stdint.h>
#include <utility>
#include <vector>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPixelShares.h"
#include "PokemonLZA_AutoStoryAdvanceArrowDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  A 4-connected blob of a mask: its bounds and its pixel count.
struct Blob{
    size_t min_x = 0;
    size_t min_y = 0;
    size_t max_x = 0;
    size_t max_y = 0;
    size_t count = 0;
};

//  An area's mask split into blobs: each pixel's blob label (0 outside the
//  mask; blob i has label i + 1).
struct BlobImage{
    size_t width = 0;
    size_t height = 0;
    std::vector<uint32_t> labels;
    std::vector<Blob> blobs;

    uint32_t label(size_t x, size_t y) const{
        return labels[y * width + x];
    }
};

//  A mask pixel no blob has taken yet.
constexpr uint32_t UNLABELLED = UINT32_MAX;

//  Labels the blob of mask pixels 4-connected to (x, y).
void flood_blob(size_t x, size_t y, BlobImage& image){
    const uint32_t label = (uint32_t)image.blobs.size() + 1;
    Blob blob{x, y, x, y, 0};
    std::vector<std::pair<size_t, size_t>> stack{{x, y}};
    image.labels[y * image.width + x] = label;
    const auto take = [&](size_t nx, size_t ny){
        uint32_t& here = image.labels[ny * image.width + nx];
        if (here == UNLABELLED){
            here = label;
            stack.emplace_back(nx, ny);
        }
    };
    while (!stack.empty()){
        const auto [cx, cy] = stack.back();
        stack.pop_back();
        blob.count++;
        blob.min_x = std::min(blob.min_x, cx);
        blob.max_x = std::max(blob.max_x, cx);
        blob.min_y = std::min(blob.min_y, cy);
        blob.max_y = std::max(blob.max_y, cy);
        if (cx > 0){
            take(cx - 1, cy);
        }
        if (cx + 1 < image.width){
            take(cx + 1, cy);
        }
        if (cy > 0){
            take(cx, cy - 1);
        }
        if (cy + 1 < image.height){
            take(cx, cy + 1);
        }
    }
    image.blobs.push_back(blob);
}

//  The pixels of `area` that `in`, split into blobs.
template <typename In>
BlobImage find_blobs(const ImageViewRGB32& area, In&& in){
    BlobImage image;
    image.width = area.width();
    image.height = area.height();
    image.labels.assign(image.width * image.height, 0);
    for (size_t y = 0; y < image.height; y++){
        for (size_t x = 0; x < image.width; x++){
            if (in(Color(area.pixel(x, y)))){
                image.labels[y * image.width + x] = UNLABELLED;
            }
        }
    }
    for (size_t y = 0; y < image.height; y++){
        for (size_t x = 0; x < image.width; x++){
            if (image.label(x, y) == UNLABELLED){
                flood_blob(x, y, image);
            }
        }
    }
    return image;
}

//  How well blob `label` fills a V across its bounds: its overlap with the
//  V over their union. Each arm runs from 10% in at the top to the middle
//  at the bottom, 22% of the width across.
double v_match(const BlobImage& image, uint32_t label){
    const Blob& blob = image.blobs[label - 1];
    const double width = double(blob.max_x - blob.min_x + 1);
    const double height = double(blob.max_y - blob.min_y + 1);
    size_t both = 0;
    size_t either = 0;
    for (size_t y = blob.min_y; y <= blob.max_y; y++){
        const double v = (double(y - blob.min_y) + 0.5) / height;
        for (size_t x = blob.min_x; x <= blob.max_x; x++){
            const double u = (double(x - blob.min_x) + 0.5) / width;
            const bool in_v = std::abs(std::abs(u - 0.5) - (0.40 * (1 - v) + 0.03)) <= 0.11;
            const bool in_blob = image.label(x, y) == label;
            if (in_v && in_blob){
                both++;
            }
            if (in_v || in_blob){
                either++;
            }
        }
    }
    return either == 0 ? 0 : double(both) / double(either);
}

//  The mask pixels of other blobs within `margin` of blob `label`'s
//  bounds, per pixel of the blob.
double stray_share(const BlobImage& image, uint32_t label, size_t margin){
    const Blob& blob = image.blobs[label - 1];
    const size_t min_x = blob.min_x > margin ? blob.min_x - margin : 0;
    const size_t min_y = blob.min_y > margin ? blob.min_y - margin : 0;
    const size_t max_x = std::min(blob.max_x + margin, image.width - 1);
    const size_t max_y = std::min(blob.max_y + margin, image.height - 1);
    size_t stray = 0;
    for (size_t y = min_y; y <= max_y; y++){
        for (size_t x = min_x; x <= max_x; x++){
            const uint32_t here = image.label(x, y);
            if (here != 0 && here != label){
                stray++;
            }
        }
    }
    return double(stray) / double(blob.count);
}

//  Whether blob `label` is the chevron, on a screen `scale` times 1080p:
//  32x19 measured, 30-33 x 18-20 on every frame with it.
bool chevron_blob(const BlobImage& image, uint32_t label, double scale){
    const Blob& blob = image.blobs[label - 1];
    const double width = double(blob.max_x - blob.min_x + 1) / scale;
    const double height = double(blob.max_y - blob.min_y + 1) / scale;
    if (width < 26 || width > 40 || height < 15 || height > 24){
        return false;
    }
    const double aspect = width / height;
    if (aspect < 1.4 || aspect > 2.1){
        return false;
    }
    return stray_share(image, label, size_t(4 * scale + 0.5)) <= 0.05 && v_match(image, label) >= 0.45;
}

//  Whether a blob of the pixels of `area` that `in` is the chevron.
template <typename In>
bool chevron_in(const ImageViewRGB32& area, double scale, In&& in){
    const BlobImage image = find_blobs(area, in);
    for (uint32_t label = 1; label <= image.blobs.size(); label++){
        if (chevron_blob(image, label, scale)){
            return true;
        }
    }
    return false;
}

} // namespace


AdvanceArrowDetector::AdvanceArrowDetector(Color color)
    : m_color(color)
    , m_area(1370.0 / 1920, 930.0 / 1080, 110.0 / 1920, 120.0 / 1080)
{}

void AdvanceArrowDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_area);
}

//  On 1920x1080 the chevron (on the ad's subtitles, on white boxes, on the
//  opening's blue box) fills the V 0.53-0.70 with at most 1% stray pixels.
//  The closest other shape in size and shape, a dark blade of scenery,
//  fills the V 0.49 but has 16% stray pixels; shapes with nothing around
//  them fill the V 0.43 or less and are out of the chevron's size.
bool AdvanceArrowDetector::detect(const ImageViewRGB32& screen){
    const double scale = double(screen.height()) / 1080;
    const ImageViewRGB32 area = extract_box_reference(screen, m_area);
    const bool white = chevron_in(area, scale, [](Color pixel){
        return pixel.red() >= 235 && pixel.green() >= 235 && pixel.blue() >= 235;
    });
    return white || chevron_in(area, scale, [](Color pixel){
        return pixel.red() <= 110 && pixel.green() <= 110 && pixel.blue() <= 110;
    });
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
