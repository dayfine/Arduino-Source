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

//  Rows span x 144-742: the choices from y 578 and 662 (68 high), "Ready!" from y 790
//  (112 high). Each strip is 100 wide from x 620, right of the text.
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

//  Highlighted: white, flat; others slate. The highlight's glow raises the next row's
//  spread up to 20.
constexpr SlateRows CONFIRM_ROWS_SLATE{
    .min_red = 20, .max_red = 55,
    .min_green = 40, .max_green = 80,
    .min_blue = 65, .max_blue = 115,
    .min_blue_over_red = 25,
    .max_spread = 30,
};

//  The dotted background between the choices and "Ready!" and right of all three:
//  (10-13, 29-59, 63-106), spread up to 51 under the highlight's glow. Dark and blue
//  with little red; the intro's other screens are not.
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


//  The language list and look picker fall on the background strips, so neither reads
//  as this screen.
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
