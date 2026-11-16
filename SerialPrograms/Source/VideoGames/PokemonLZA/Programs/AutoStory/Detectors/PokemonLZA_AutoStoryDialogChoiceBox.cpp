/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <vector>
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryDialogChoiceBox.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Rows read from the bottom up before the reader stops looking.
constexpr size_t MAX_CHOICE_ROWS = 6;
//  A menu has two rows at least. One white strip alone is not one: light
//  pavement reads as a highlighted row (ArrivalToAd 0038).
constexpr size_t MIN_CHOICE_ROWS = 2;

//  The menu's geometry at 1920 x 1080. Measured on the console on the
//  tourist's two rows (Traditional Chinese), the Research Lab elevator's
//  three (Japanese) and Villy's two at 01.02 (Italian). The panel is
//  anchored at its bottom right: it ends at x 1728, y 808 whatever its
//  rows, and grows left to fit its longest row (x 1408 in Japanese, 1343
//  in Italian). The highlight runs from 10 right of the panel's left to
//  x 1715; the bottom row's from y 732 to 800, each row above 80 higher.
//  The rows' text starts 35 right of the panel's left and ends at x 1690
//  at most (Italian's longest row: 1688).
constexpr double ROW_PITCH = 80.0 / 1080;
constexpr double STRIP_TOP = 748.0 / 1080;
constexpr double STRIP_HEIGHT = 40.0 / 1080;
constexpr double TEXT_END_X = 1690.0 / 1920;
//  Where the leftward scan for the highlight's left end starts, and the
//  bottom row's highlight 4 below its top and 4 above its bottom: few
//  glyphs reach both.
constexpr double HIGHLIGHT_SCAN_X = 1700.0 / 1920;
constexpr double HIGHLIGHT_TOP_BAND_Y = 736.0 / 1080;
constexpr double HIGHLIGHT_BOTTOM_BAND_Y = 796.0 / 1080;
//  A plain row's share of bright text pixels: 0.07 to 0.2 on the console,
//  0 on the elevator's wall above its top row.
constexpr double MIN_TEXT_SHARE = 0.005;

//  Right of any row's text, inside the highlight.
ImageFloatBox right_strip(size_t from_bottom){
    return ImageFloatBox(1698.0 / 1920, STRIP_TOP - ROW_PITCH * from_bottom, 14.0 / 1920, STRIP_HEIGHT);
}

//  Where a row's text can be, from 10 right of the highlight's left end
//  (`highlight_left`, a share of the width) to the text's right limit.
ImageFloatBox text_band(size_t from_bottom, double highlight_left){
    const double left = highlight_left + 10.0 / 1920;
    return ImageFloatBox(left, STRIP_TOP - ROW_PITCH * from_bottom, TEXT_END_X - left, STRIP_HEIGHT);
}

bool bright(uint32_t pixel, uint32_t min_channel){
    const uint32_t red = (pixel >> 16) & 0xff;
    const uint32_t green = (pixel >> 8) & 0xff;
    const uint32_t blue = pixel & 0xff;
    return red >= min_channel && green >= min_channel && blue >= min_channel;
}

enum class ChoiceSlot{
    HIGHLIGHTED,
    PANEL,
    OTHER,
};

//  A slot's right strip: the highlighted row 255 white, flat; a plain row
//  the panel's dark blue, (35-45, 60-73, 108-120), spread 3-7. The wall
//  above the elevator's top row is as dark and blue, (23-24, 58-62,
//  102-107); `row_has_text` tells it from a row.
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
    return dark_blue ? ChoiceSlot::PANEL : ChoiceSlot::OTHER;
}

//  The highlight's left end on the row `from_bottom` slots up, as a share
//  of the width: scanned left from inside its right end while its top or
//  bottom band is white.
double highlight_left(const ImageViewRGB32& frame, size_t from_bottom){
    const double pitch = ROW_PITCH * from_bottom;
    const size_t top_y = (size_t)((HIGHLIGHT_TOP_BAND_Y - pitch) * frame.height());
    const size_t bottom_y = (size_t)((HIGHLIGHT_BOTTOM_BAND_Y - pitch) * frame.height());
    size_t x = (size_t)(HIGHLIGHT_SCAN_X * frame.width());
    while (x > 0 && (bright(frame.pixel(x - 1, top_y), 200) || bright(frame.pixel(x - 1, bottom_y), 200))){
        x--;
    }
    return (double)x / frame.width();
}

//  Whether a plain row's text band has text: white glyphs on the panel,
//  whatever the language or the row's length.
bool row_has_text(const ImageViewRGB32& frame, size_t from_bottom, double highlight_left){
    const ImageViewRGB32 band = extract_box_reference(frame, text_band(from_bottom, highlight_left));
    const size_t area = band.width() * band.height();
    if (area == 0){
        return false;
    }
    size_t bright_pixels = 0;
    for (size_t y = 0; y < band.height(); y++){
        for (size_t x = 0; x < band.width(); x++){
            if (bright(band.pixel(x, y), 180)){
                bright_pixels++;
            }
        }
    }
    return (double)bright_pixels / area >= MIN_TEXT_SHARE;
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
    //  The menu's slots from the bottom up, highlighted or panel blue.
    std::vector<ChoiceSlot> slots;
    std::optional<size_t> first_highlighted;
    while (slots.size() < MAX_CHOICE_ROWS){
        const ChoiceSlot slot = classify_slot(frame, slots.size());
        if (slot == ChoiceSlot::OTHER){
            break;
        }
        if (slot == ChoiceSlot::HIGHLIGHTED && !first_highlighted){
            first_highlighted = slots.size();
        }
        slots.push_back(slot);
    }
    if (!first_highlighted){
        return std::nullopt;
    }
    //  The rows end at the first blue slot with no text.
    const double left = highlight_left(frame, *first_highlighted);
    size_t count = 0;
    size_t highlighted = 0;
    for (; count < slots.size(); count++){
        if (slots[count] == ChoiceSlot::HIGHLIGHTED){
            highlighted++;
        }else if (!row_has_text(frame, count, left)){
            break;
        }
    }
    if (highlighted != 1 || count < MIN_CHOICE_ROWS){
        return std::nullopt;
    }
    return DialogChoiceRows{.count = count, .selected = count - 1 - *first_highlighted};
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
