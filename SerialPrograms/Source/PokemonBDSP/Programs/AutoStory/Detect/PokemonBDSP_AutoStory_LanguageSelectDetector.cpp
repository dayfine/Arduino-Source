/*  BDSP AutoStory - Language Select Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonBDSP_AutoStory_LanguageSelectDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  A strip inside each button, left of its text (1080p: x 330-460 on the left
//  column, 1040-1170 on the right; 60 px tall around each row's centre).
const double COLUMN_X[2] = {0.172, 0.542};
const double ROW_CENTER_Y[5] = {0.154, 0.280, 0.404, 0.528, 0.652};
const double STRIP_WIDTH = 0.068;
const double STRIP_HEIGHT = 0.056;

//  Buttons are flat: the frame's measured stddev sum is 2 to 6.
const double MAX_STDDEV_SUM = 15;

enum class ButtonLook{ GREY, BLUE, OTHER };

ImageFloatBox button_strip(size_t item){
    return ImageFloatBox(COLUMN_X[item % 2], ROW_CENTER_Y[item / 2] - STRIP_HEIGHT / 2, STRIP_WIDTH, STRIP_HEIGHT);
}

//  Grey (225, 233, 238) and blue (167, 206, 255) on the frame. The background
//  between the buttons is a bluer (203, 232, 253), not grey.
ButtonLook button_look(const ImageViewRGB32& screen, size_t item){
    const ImageStats stats = image_stats(extract_box_reference(screen, button_strip(item)));
    if (stats.stddev.sum() > MAX_STDDEV_SUM){
        return ButtonLook::OTHER;
    }
    const FloatPixel& c = stats.average;
    const double low = std::min({c.r, c.g, c.b});
    const double high = std::max({c.r, c.g, c.b});
    if (low >= 200 && high - low <= 25){
        return ButtonLook::GREY;
    }
    if (c.b >= 230 && c.r >= 120 && c.b - c.r >= 50){
        return ButtonLook::BLUE;
    }
    return ButtonLook::OTHER;
}

} // namespace


std::optional<size_t> language_select_cursor(const ImageViewRGB32& screen){
    std::optional<size_t> cursor;
    for (size_t item = 0; item < LANGUAGE_SELECT_BUTTONS; item++){
        switch (button_look(screen, item)){
        case ButtonLook::GREY:
            break;
        case ButtonLook::BLUE:
            if (cursor){
                return std::nullopt;
            }
            cursor = item;
            break;
        case ButtonLook::OTHER:
            return std::nullopt;
        }
    }
    return cursor;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
