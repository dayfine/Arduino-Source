/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a dialogue's selection arrow can be, and a reader for the choice rows
 *  (how many, and which one is highlighted).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_H

#include <cstddef>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Where a dialogue's selection arrow can be, widened left to x 960 for long languages.
const ImageFloatBox& dialog_choice_box();

//  Row count and the highlighted row, 0 the top.
struct DialogChoiceRows{
    size_t count = 0;
    size_t selected = 0;
};

//  The panel is anchored at its bottom right, so rows are counted from the bottom up,
//  each highlighted (white right of the text) or plain (blue, with text), until a slot
//  is neither. None for fewer than two rows or unless exactly one is highlighted.
std::optional<DialogChoiceRows> read_dialog_choice_rows(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
