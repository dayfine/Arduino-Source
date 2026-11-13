/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryLanguageList.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

enum class ListBox{
    HIGHLIGHTED,
    PLAIN,
    OTHER,
};

//  A strip at the left end of entry `index`'s box, left of its text and
//  right of the cursor's arrow. The boxes are 576 x 84 at 1920 x 1080, the
//  columns' left edges at x 347 and 996, the rows' tops 108 apart from
//  y 342; the strip is 65 x 54 from 38 right of the edge and 15 down.
ImageFloatBox language_box_strip(size_t index){
    const size_t column = index / LANGUAGE_LIST_ROWS;
    const size_t row = index % LANGUAGE_LIST_ROWS;
    return ImageFloatBox(column == 0 ? 0.2005 : 0.5365, 0.331 + 0.1 * (double)row, 0.034, 0.05);
}

//  Measured on console B (2026-11-13): the highlighted box is 255 white,
//  flat; the others slate blue, about (54, 60-76, 82-107), each channel's
//  spread under 3.
ListBox classify_list_box(const ImageStats& stats){
    if (stats.count == 0){
        return ListBox::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 30){
        return ListBox::HIGHLIGHTED;
    }
    const bool slate =
        average.r >= 35 && average.r <= 90 &&
        average.g >= 45 && average.g <= 110 &&
        average.b >= 65 && average.b <= 140 &&
        average.b >= average.r + 15;
    if (slate && spread <= 20){
        return ListBox::PLAIN;
    }
    return ListBox::OTHER;
}

//  The dark blue background between the columns (about (9, 24, 56)).
bool column_gap_dark(const ImageViewRGB32& frame){
    const ImageStats gap = image_stats(extract_box_reference(frame, ImageFloatBox(0.4896, 0.331, 0.0208, 0.447)));
    return gap.count > 0 && gap.average.sum() / 3 < 70;
}

} // namespace


std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame){
    if (!frame || frame.width() < 160 || frame.height() < 90){
        return std::nullopt;
    }
    if (!column_gap_dark(frame)){
        return std::nullopt;
    }
    std::optional<size_t> highlighted;
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        const ImageStats stats = image_stats(extract_box_reference(frame, language_box_strip(index)));
        switch (classify_list_box(stats)){
        case ListBox::HIGHLIGHTED:
            if (highlighted){
                return std::nullopt;
            }
            highlighted = index;
            break;
        case ListBox::PLAIN:
            break;
        case ListBox::OTHER:
            return std::nullopt;
        }
    }
    return highlighted;
}

bool is_language_list(const ImageViewRGB32& frame){
    return read_language_list_cursor(frame).has_value();
}

bool is_look_picker(const ImageViewRGB32&){
    return false;
}

bool is_keyboard(const ImageViewRGB32&){
    return false;
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
