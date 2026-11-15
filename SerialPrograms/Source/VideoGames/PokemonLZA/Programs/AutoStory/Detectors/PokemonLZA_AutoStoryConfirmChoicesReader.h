/*  PokemonLZA AutoStory - Confirm Choices reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's "Confirm Choices" screen, where checkpoint 01.00 starts:
 *  the PROFILE screen after the name keyboard, with its rows "Change hair
 *  color", "Change look" and "Ready!".
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConfirmChoicesReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryConfirmChoicesReader_H

#include <cstddef>
#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  "Confirm Choices" has three rows: "Change hair color" (0, highlighted as
//  it opens), "Change look" and "Ready!", the last.
constexpr size_t CONFIRM_CHOICES_READY_ROW = 2;

//  On "Confirm Choices", the highlighted row, 0 to
//  CONFIRM_CHOICES_READY_ROW: the one white box of the three, the others
//  slate. Nullopt for any other frame, an empty one included.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame);

//  "Confirm Choices", with one row highlighted.
bool is_confirm_choices(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
