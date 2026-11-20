/*  PokemonLZA AutoStory - Highlighted row
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryHighlightedRow.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

enum class RowBox{
    HIGHLIGHTED,
    PLAIN,
    OTHER,
};

RowBox classify_row(const ImageStats& stats, const SlateRows& slate){
    if (stats.count == 0){
        return RowBox::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 30){
        return RowBox::HIGHLIGHTED;
    }
    const bool plain =
        average.r >= slate.min_red && average.r <= slate.max_red &&
        average.g >= slate.min_green && average.g <= slate.max_green &&
        average.b >= slate.min_blue && average.b <= slate.max_blue &&
        average.b >= average.r + slate.min_blue_over_red &&
        spread <= slate.max_spread;
    return plain ? RowBox::PLAIN : RowBox::OTHER;
}

} // namespace


std::optional<size_t> highlighted_row(const ImageViewRGB32& frame, std::span<const ImageFloatBox> strips, const SlateRows& slate){
    std::optional<size_t> highlighted;
    for (size_t row = 0; row < strips.size(); row++){
        switch (classify_row(image_stats(extract_box_reference(frame, strips[row])), slate)){
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
