/*  BDSP AutoStory - Name Limit Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch keyboard's character counter, "0/12" under the name field's
 *  right end, read for the most characters the game takes: BDSP takes fewer
 *  in some languages, and the keyboard drops the presses past it. Measured
 *  on a 1920x1080 Switch 1 capture of the same keyboard in Legends: Z-A
 *  ("0/6", grey on dark blue, about x 1275-1335, y 357-385).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_NameLimitReader_H
#define PokemonAutomation_PokemonBDSP_AutoStory_NameLimitReader_H

#include <cstddef>
#include <optional>
#include <string_view>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The limit in a counter's text ("0/6" gives 6; spaces and a trailing
//  newline are allowed), or nullopt when the text is not "<digits>/<digits>".
std::optional<size_t> parse_keyboard_counter(std::string_view text);

//  The keyboard's character limit, read with OCR from its counter; nullopt
//  when the counter does not read as one.
std::optional<size_t> read_keyboard_name_limit(const ImageViewRGB32& screen);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
