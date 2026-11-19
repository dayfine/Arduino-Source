/*  PokemonLZA AutoStory - Confirm Choices reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's "Confirm Choices" screen (PROFILE), where checkpoint 01.00b starts.
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


//  Rows: "Change hair color" (0), "Change look" and "Ready!".
constexpr size_t CONFIRM_CHOICES_READY_ROW = 2;

//  The highlighted row on "Confirm Choices"; nullopt for any other frame.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
