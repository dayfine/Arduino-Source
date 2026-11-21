/*  PokemonLZA AutoStory - Objective marker reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "PokemonLZA_AutoStoryObjectiveMarkerReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


std::optional<ObjectiveMarker> read_objective_marker(Logger& logger, const ImageViewRGB32& screen){
    if (!screen){
        return std::nullopt;
    }
    //  The hexagon's orange, about (245, 175, 40) on console B: the largest
    //  hexagon-sized blob outside the minimap (its marker is there too).
    const double scale = screen.height() / 1080.0;
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(screen, 220, 255, 140, 215, 0, 90);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)(500 * scale * scale));
    std::optional<WaterfillObject> best;
    WaterfillObject object;
    while (iter->find_next(object, false)){
        //  The minimap: a circle of radius 125 around (180, 180) on
        //  1920x1080. A box around it also covered the real marker when it
        //  sat at the left edge, high up (console B, 2026-11-10).
        const double dx = object.center_of_gravity_x() / scale - 180;
        const double dy = object.center_of_gravity_y() / scale - 180;
        const bool in_minimap = dx * dx + dy * dy < 140 * 140;
        const bool in_party_bar = object.center_of_gravity_y() / screen.height() > 0.70;
        const double aspect = object.aspect_ratio();
        if (in_minimap || in_party_bar || object.area > 4000 * scale * scale || aspect < 0.6 || aspect > 1.4){
            continue;
        }
        if (!best || object.area > best->area){
            best = object;
        }
    }
    if (!best){
        return std::nullopt;
    }
    //  The distance: white digits just above the hexagon, up to its width
    //  out each side, about half its height tall.
    const double w = (double)best->width();
    const double h = (double)best->height();
    const ImagePixelBox digits(
        (size_t)std::max(0.0, best->min_x - w), (size_t)std::max(0.0, best->min_y - 0.95 * h),
        (size_t)std::min((double)screen.width(), best->max_x + w), (size_t)std::max(1.0, best->min_y - 0.10 * h)
    );
    ObjectiveMarker marker;
    marker.x = (best->min_x + best->max_x) / 2.0 / screen.width();
    marker.distance = OCR::read_number_waterfill(logger, extract_box_reference(screen, digits), 0xffd0d0d0, 0xffffffff);
    return marker;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
