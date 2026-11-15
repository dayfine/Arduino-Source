/*  BDSP AutoStory - Title Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/ImageFilter.h"
#include "PokemonBDSP_AutoStory_TitleDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  The band under the logo that holds the red line, wide enough for the
//  longest language's text (French: x 0.34 to 0.65, y 0.76 to 0.81).
const ImageFloatBox PROMPT_BAND(0.30, 0.73, 0.40, 0.10);
//  The starfield's bottom corners, dark on the title (each channel's mean
//  under 20 on console B in Japanese and French).
const ImageFloatBox BOTTOM_LEFT(0.00, 0.88, 0.10, 0.12);
const ImageFloatBox BOTTOM_RIGHT(0.90, 0.88, 0.10, 0.12);
constexpr double DARK_CORNER = 40;

//  The line's red (about 200, 40, 45), and its share of the band: about
//  0.09 in Japanese and French; a solid red button fills most of it.
constexpr uint32_t PROMPT_RED_MIN = combine_rgb(150, 0, 0);
constexpr uint32_t PROMPT_RED_MAX = combine_rgb(255, 90, 90);
constexpr double MIN_RED_SHARE = 0.04;
constexpr double MAX_RED_SHARE = 0.50;

bool dark(const ImageViewRGB32& screen, const ImageFloatBox& box){
    const FloatPixel mean = image_stats(extract_box_reference(screen, box)).average;
    return std::max({mean.r, mean.g, mean.b}) < DARK_CORNER;
}

} // namespace


bool title_screen_shown(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return false;
    }
    if (!dark(screen, BOTTOM_LEFT) || !dark(screen, BOTTOM_RIGHT)){
        return false;
    }
    const ImageViewRGB32 band = extract_box_reference(screen, PROMPT_BAND);
    const size_t pixels = band.width() * band.height();
    if (pixels == 0){
        return false;
    }
    size_t red = 0;
    filter_rgb32_range(red, band, PROMPT_RED_MIN, PROMPT_RED_MAX, COLOR_BLACK, false);
    const double share = (double)red / pixels;
    return share >= MIN_RED_SHARE && share <= MAX_RED_SHARE;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
