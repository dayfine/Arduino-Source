/*  BDSP AutoStory - Language Select Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's language select, shown on a new game's first boot: ten buttons in
 *  two columns of five over a crystal background, "Select the language you
 *  would like to play in." below them. The cursor's button is blue with a red
 *  outline, the others light grey (lab-B 2026-11-09, 1080p).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LanguageSelectDetector_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LanguageSelectDetector_H

#include <cstddef>
#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The buttons, numbered row by row: English 0, its right neighbour 1, the
//  next row 2 and 3, and so on (BDSP_LANGUAGE_GRID).
inline constexpr size_t LANGUAGE_SELECT_BUTTONS = 10;

//  The button the cursor is on, or nullopt when `screen` is not the language
//  select: every button must read as plain grey but exactly one, blue.
std::optional<size_t> language_select_cursor(const ImageViewRGB32& screen);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
