/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryDialogChoiceBox.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Rows read from the bottom up before the reader stops looking.
constexpr size_t MAX_CHOICE_ROWS = 6;

//  The row pitch and the bottom row's strips at 1920 x 1080. Measured on
//  the console on the tourist's two rows (Traditional Chinese)
//  and the Research Lab elevator's three (Japanese): the panel x 1408 to
//  1728 ends at y 808 in both, its highlight x 1418 to 1715, the bottom
//  row's from y 732 to 800, each row above 80 higher. The text starts at
//  x 1443 on every row.
constexpr double ROW_PITCH = 80.0 / 1080;
constexpr double STRIP_TOP = 748.0 / 1080;
constexpr double STRIP_HEIGHT = 40.0 / 1080;

//  Right of the row's text, inside the highlight.
ImageFloatBox right_strip(size_t from_bottom){
    return ImageFloatBox(1650.0 / 1920, STRIP_TOP - ROW_PITCH * from_bottom, 55.0 / 1920, STRIP_HEIGHT);
}

//  Where every row's text starts.
ImageFloatBox text_strip(size_t from_bottom){
    return ImageFloatBox(1445.0 / 1920, STRIP_TOP - ROW_PITCH * from_bottom, 80.0 / 1920, STRIP_HEIGHT);
}

enum class ChoiceSlot{
    HIGHLIGHTED,
    PLAIN,
    OTHER,
};

//  A slot's right strip: the highlighted row 255 white, flat; a plain row
//  the panel's dark blue, (35-45, 60-73, 108-120), spread 4-5. Its text
//  strip: spread 220-240 on every row. Above the elevator's top row, the
//  wall is as dark and blue, (23-24, 58-60, 102-104), but its text strip
//  is flat (spread 5); above the tourist's, pavement, spread 34-36.
ChoiceSlot classify_slot(const ImageViewRGB32& frame, size_t from_bottom){
    const ImageStats right = image_stats(extract_box_reference(frame, right_strip(from_bottom)));
    if (right.count == 0 || right.stddev.sum() > 30){
        return ChoiceSlot::OTHER;
    }
    const FloatPixel& average = right.average;
    if (average.r >= 200 && average.g >= 200 && average.b >= 200){
        return ChoiceSlot::HIGHLIGHTED;
    }
    const bool dark_blue =
        average.r <= 70 && average.g <= 100 &&
        average.b >= 80 && average.b <= 150 &&
        average.b >= average.r + 40;
    if (!dark_blue){
        return ChoiceSlot::OTHER;
    }
    const ImageStats text = image_stats(extract_box_reference(frame, text_strip(from_bottom)));
    if (text.count == 0 || text.stddev.sum() < 100){
        return ChoiceSlot::OTHER;
    }
    return ChoiceSlot::PLAIN;
}

} // namespace


const ImageFloatBox& dialog_choice_box(){
    static const ImageFloatBox box{0.654308, 0.481553, 0.295529, 0.312621};
    return box;
}

std::optional<DialogChoiceRows> read_dialog_choice_rows(const ImageViewRGB32& frame){
    if (!frame || frame.width() < 192 || frame.height() < 108){
        return std::nullopt;
    }
    size_t count = 0;
    std::optional<size_t> highlighted_from_bottom;
    for (; count < MAX_CHOICE_ROWS; count++){
        const ChoiceSlot slot = classify_slot(frame, count);
        if (slot == ChoiceSlot::OTHER){
            break;
        }
        if (slot == ChoiceSlot::HIGHLIGHTED){
            if (highlighted_from_bottom){
                return std::nullopt;
            }
            highlighted_from_bottom = count;
        }
    }
    if (!highlighted_from_bottom){
        return std::nullopt;
    }
    return DialogChoiceRows{.count = count, .selected = count - 1 - *highlighted_from_bottom};
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
