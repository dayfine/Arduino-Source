/*  PokemonLZA AutoStory - Confirm Choices reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryConfirmChoicesReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

enum class RowBox{
    HIGHLIGHTED,
    PLAIN,
    OTHER,
};

double brightness(const ImageStats& stats){
    return stats.average.sum() / 3;
}

//  The right ends of the "Confirm Choices" rows, right of their text: the
//  boxes span x 144 to 742 at 1920 x 1080, the two choices from y 578 and
//  662 (68 high), "Ready!" from y 790 (112 high). Each strip is 100 wide
//  from x 620.
ImageFloatBox confirm_row_strip(size_t row){
    switch (row){
    case 0:
        return ImageFloatBox(620.0 / 1920, 592.0 / 1080, 100.0 / 1920, 40.0 / 1080);
    case 1:
        return ImageFloatBox(620.0 / 1920, 676.0 / 1080, 100.0 / 1920, 40.0 / 1080);
    default:
        return ImageFloatBox(620.0 / 1920, 820.0 / 1080, 100.0 / 1920, 60.0 / 1080);
    }
}

//  A "Confirm Choices" row's strip: the highlighted row white, flat; the
//  others slate. The highlight's glow pulses over the row next to it: on
//  console B's frames in ten languages (2026-11-13) that row's spread went
//  up to 20.
RowBox classify_confirm_row(const ImageStats& stats){
    if (stats.count == 0){
        return RowBox::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 30){
        return RowBox::HIGHLIGHTED;
    }
    const bool slate =
        average.r >= 20 && average.r <= 55 &&
        average.g >= 40 && average.g <= 80 &&
        average.b >= 65 && average.b <= 115 &&
        average.b >= average.r + 25;
    if (slate && spread <= 30){
        return RowBox::PLAIN;
    }
    return RowBox::OTHER;
}

//  The dark blue, dotted background round the rows: between the choices and
//  "Ready!", and right of all three. The highlight's blue glow reaches 40
//  pixels out and fills the gap under "Change looks" when that row is
//  highlighted: (12-13, 56-59, 102-106), spread up to 51, against the plain
//  background's (10-11, 29-37, 63-75), spread up to 31 (console B, ten
//  languages, 2026-11-13). Both are dark and blue with little red; the
//  intro's other screens are not.
bool confirm_background_dark(const ImageViewRGB32& frame){
    const ImageFloatBox boxes[] = {
        ImageFloatBox(620.0 / 1920, 742.0 / 1080, 100.0 / 1920, 40.0 / 1080),
        ImageFloatBox(770.0 / 1920, 580.0 / 1080, 30.0 / 1920, 320.0 / 1080),
    };
    for (const ImageFloatBox& box : boxes){
        const ImageStats stats = image_stats(extract_box_reference(frame, box));
        if (stats.count == 0 || brightness(stats) >= 90 || stats.stddev.sum() > 80){
            return false;
        }
        if (stats.average.r > 30 || stats.average.b < stats.average.r + 40){
            return false;
        }
    }
    return true;
}

} // namespace


//  Measured on console B (2026-11-13), "Confirm Choices" as it opens: the
//  highlighted row 255 white, flat; the others slate, (35-36, 52-61,
//  82-95), spread under 10; the background (10, 29-31, 64-66), spread under
//  25. The intro's language list and look picker fall on the background's
//  strips, so neither reads as this screen.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame){
    if (!frame || frame.width() < 160 || frame.height() < 90){
        return std::nullopt;
    }
    if (!confirm_background_dark(frame)){
        return std::nullopt;
    }
    std::optional<size_t> highlighted;
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        const ImageStats stats = image_stats(extract_box_reference(frame, confirm_row_strip(row)));
        switch (classify_confirm_row(stats)){
        case RowBox::HIGHLIGHTED:
            if (highlighted){
                return std::nullopt;
            }
            highlighted = row;
            break;
        case RowBox::PLAIN:
            break;
        case RowBox::OTHER:
            return std::nullopt;
        }
    }
    return highlighted;
}

bool is_confirm_choices(const ImageViewRGB32& frame){
    return read_confirm_choices_cursor(frame).has_value();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
