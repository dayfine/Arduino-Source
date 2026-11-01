/*  Name List Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <vector>
#include "Kernels/Waterfill/Kernels_Waterfill.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "CommonTools/ImageMatch/ExactImageMatcher.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "PokemonBDSP_NameListDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{
//  Row centers, as a fraction of the screen height (1080p: 307, 370, 433, ...).
const double FIRST_ROW_Y = 0.284;
const double ROW_STEP_Y  = 0.0587;
const int ROWS = 5;

//  The menu's selection arrows in `image`, as PokemonBDSP_SelectionArrow.cpp
//  finds them: that file keeps its finder out of its header, so the same
//  waterfill and template match are repeated here rather than changing it.
const ImageMatch::ExactImageMatcher& name_list_arrow(){
    static ImageMatch::ExactImageMatcher matcher(RESOURCE_PATH() + "PokemonBDSP/SelectionArrow.png");
    return matcher;
}
std::vector<ImagePixelBox> find_name_list_arrows(const ImageViewRGB32& image){
    using namespace Kernels;
    using namespace Kernels::Waterfill;
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_max(image, 200, 200, 200);
    std::vector<WaterfillObject> objects = find_objects_inplace(matrix, 200);
    std::vector<ImagePixelBox> ret;
    for (const WaterfillObject& object : objects){
        if (object.width() > object.height() || object.height() > 3 * object.width()){
            continue;
        }
        if (name_list_arrow().rmsd(extract_box_reference(image, object)) <= 90){
            ret.emplace_back(object.min_x, object.min_y, object.max_x, object.max_y);
        }
    }
    return ret;
}
}


NameListDetector::NameListDetector(Color color)
    : m_color(color)
    , m_arrows(0.137, 0.245, 0.030, 0.310)
    , m_panel(0.140, 0.245, 0.140, 0.300)
{}
void NameListDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_arrows);
    items.add(m_color, m_panel);
}
bool NameListDetector::detect(const ImageViewRGB32& screen){
    return selected_row(screen) >= 0;
}
int NameListDetector::selected_row(const ImageViewRGB32& screen) const{
    //  The list is a white panel with a little dark text. A thin margin strip is not
    //  enough: the border's antialiasing reaches into it on some frames.
    const ImageViewRGB32 panel = extract_box_reference(screen, m_panel);
    size_t white = 0;
    for (size_t y = 0; y < panel.height(); y += 2){
        for (size_t x = 0; x < panel.width(); x += 2){
            const uint32_t p = panel.pixel(x, y);
            white += ((p >> 16) & 0xff) > 220 && ((p >> 8) & 0xff) > 220 && (p & 0xff) > 220;
        }
    }
    const size_t samples = ((panel.width() + 1) / 2) * ((panel.height() + 1) / 2);
    if (samples == 0 || white < samples * 6 / 10){
        return -1;
    }
    ImageViewRGB32 area = extract_box_reference(screen, m_arrows);
    std::vector<ImagePixelBox> arrows = find_name_list_arrows(area);
    if (arrows.size() != 1){
        return -1;
    }
    const double center_y = m_arrows.y +
        m_arrows.height * (arrows[0].min_y + arrows[0].max_y) / 2.0 / (double)area.height();
    const int row = (int)std::lround((center_y - FIRST_ROW_Y) / ROW_STEP_Y);
    return row >= 0 && row < ROWS ? row : -1;
}


}
}
}
