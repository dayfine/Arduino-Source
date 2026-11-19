/*  PokemonLZA AutoStory - Highlighted row
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The intro's menus (the language list, "Confirm Choices") highlight their
 *  cursor's row white among slate rows.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryHighlightedRow_H
#define PokemonAutomation_PokemonLZA_AutoStoryHighlightedRow_H

#include <cstddef>
#include <optional>
#include <span>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Plain rows: blue over red by `min_blue_over_red`, channel spreads summing to
//  `max_spread` at most.
struct SlateRows{
    double min_red;
    double max_red;
    double min_green;
    double max_green;
    double min_blue;
    double max_blue;
    double min_blue_over_red;
    double max_spread;
};

//  The one white, flat strip (channels 200+, spreads summing <= 30) among `slate` ones;
//  nullopt when none or two are white, or a strip is neither.
std::optional<size_t> highlighted_row(const ImageViewRGB32& frame, std::span<const ImageFloatBox> strips, const SlateRows& slate);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
