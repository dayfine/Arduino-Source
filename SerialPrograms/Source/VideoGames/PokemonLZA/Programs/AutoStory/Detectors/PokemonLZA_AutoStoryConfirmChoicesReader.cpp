/*  PokemonLZA AutoStory - Confirm Choices reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryConfirmChoicesReader.h"
#include "PokemonLZA_AutoStoryHighlightedRow.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

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
//  the console's frames in ten languages that row's spread went
//  up to 20.
constexpr SlateRows CONFIRM_ROWS_SLATE{
    .min_red = 20, .max_red = 55,
    .min_green = 40, .max_green = 80,
    .min_blue = 65, .max_blue = 115,
    .min_blue_over_red = 25,
    .max_spread = 30,
};

//  The dark blue, dotted background round the rows: between the choices and
//  "Ready!", and right of all three. The highlight's blue glow reaches 40
//  pixels out and fills the gap under "Change looks" when that row is
//  highlighted: (12-13, 56-59, 102-106), spread up to 51, against the plain
//  background's (10-11, 29-37, 63-75), spread up to 31 (the console, ten
//  languages). Both are dark and blue with little red; the
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


//  Measured on the console, "Confirm Choices" as it opens: the
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
    const std::array<ImageFloatBox, CONFIRM_CHOICES_READY_ROW + 1> strips{confirm_row_strip(0), confirm_row_strip(1), confirm_row_strip(2)};
    return highlighted_row(frame, strips, CONFIRM_ROWS_SLATE);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
