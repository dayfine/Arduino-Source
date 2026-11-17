/*  PokemonLZA AutoStory - Party slots detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPartySlotsDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  On 1920x1080 (console B, 2026-11-16, German 02.05 with a party of one):
//  an empty slot's frame at x 78 + 76 * i for slot i (from 0), y 954, 66 px
//  square, its edges 2 px wide. The last slot's at x 458-523, y 954-1019.
constexpr double LAST_SLOT_X = 458;
constexpr double SLOT_Y = 954;
constexpr double SLOT_SIZE = 66;
constexpr double LINE = 2;
//  The strips each edge is held against: 3 px wide, 2 px off the line on
//  either side. Each edge is read along its middle, 8 px in from the
//  corners, which are rounded.
constexpr double STRIP = 3;
constexpr double GAP = 2;
constexpr double CORNER = 8;

//  A frame's line is the scene under a light veil, its inside the scene
//  darkened. On the six frames with the last slot empty (a dark lobby to
//  bright ground) every edge's line is lighter than inside by 21-60 and
//  than outside by 10-61; on 686 cutscene frames of 01.00-01.03 (no party
//  bar) by 2 at most inside.
constexpr double INSIDE_MARGIN = 10;
constexpr double OUTSIDE_MARGIN = 0;

ImageFloatBox pixel_box(double x, double y, double width, double height){
    return {x / 1920, y / 1080, width / 1920, height / 1080};
}

double brightness(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return image_average(extract_box_reference(screen, box)).sum() / 3;
}

} // namespace


PartySlotsDetector::PartySlotsDetector(Color color)
    : m_color(color)
    , m_last_slot(pixel_box(LAST_SLOT_X, SLOT_Y, SLOT_SIZE, SLOT_SIZE))
{
    const double left = LAST_SLOT_X;
    const double right = LAST_SLOT_X + SLOT_SIZE - LINE;
    const double top = SLOT_Y;
    const double bottom = SLOT_Y + SLOT_SIZE - LINE;
    const double span = SLOT_SIZE - 2 * CORNER;
    const double step = LINE + GAP;
    m_last_frame = {
        Edge{
            .line = pixel_box(left + CORNER, top, span, LINE),
            .inside = pixel_box(left + CORNER, top + step, span, STRIP),
            .outside = pixel_box(left + CORNER, top - GAP - STRIP, span, STRIP),
        },
        Edge{
            .line = pixel_box(left + CORNER, bottom, span, LINE),
            .inside = pixel_box(left + CORNER, bottom - GAP - STRIP, span, STRIP),
            .outside = pixel_box(left + CORNER, bottom + step, span, STRIP),
        },
        Edge{
            .line = pixel_box(left, top + CORNER, LINE, span),
            .inside = pixel_box(left + step, top + CORNER, STRIP, span),
            .outside = pixel_box(left - GAP - STRIP, top + CORNER, STRIP, span),
        },
        Edge{
            .line = pixel_box(right, top + CORNER, LINE, span),
            .inside = pixel_box(right - GAP - STRIP, top + CORNER, STRIP, span),
            .outside = pixel_box(right + step, top + CORNER, STRIP, span),
        },
    };
}

void PartySlotsDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_last_slot);
}

bool PartySlotsDetector::detect(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return false;
    }
    return framed(screen, m_last_frame);
}

bool PartySlotsDetector::framed(const ImageViewRGB32& screen, const std::array<Edge, 4>& frame){
    for (const Edge& edge : frame){
        const double line = brightness(screen, edge.line);
        if (line - brightness(screen, edge.inside) < INSIDE_MARGIN || line - brightness(screen, edge.outside) < OUTSIDE_MARGIN){
            return false;
        }
    }
    return true;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
