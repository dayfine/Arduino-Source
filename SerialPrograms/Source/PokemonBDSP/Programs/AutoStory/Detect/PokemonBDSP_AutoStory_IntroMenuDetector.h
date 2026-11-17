/*  BDSP AutoStory - Intro Menu Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The two menus of a new game's intro, each a grid of flat cards over a
 *  crystal background, the cursor's card blue and the others light grey
 *  (lab-B 2026-11-09, 1080p):
 *  - the language select: ten buttons in two columns of five, "Select the
 *    language you would like to play in." below;
 *  - the look select: two rows of four portraits, "Choose your photo."
 *    below. Top row: Lucas light, Lucas dark, Dawn dark-haired, Dawn dark;
 *    bottom row: Lucas blond, Lucas brown, Dawn blond, Dawn brown-haired.
 *  Cards are numbered row by row, as MenuGrid reads them.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_IntroMenuDetector_H
#define PokemonAutomation_PokemonBDSP_AutoStory_IntroMenuDetector_H

#include <cstddef>
#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The language select's buttons: English 0, its right neighbour 1, the next
//  row 2 and 3, and so on.
inline constexpr size_t LANGUAGE_SELECT_BUTTONS = 10;
//  The look select's portraits: the top row 0 to 3, the bottom row 4 to 7.
inline constexpr size_t LOOK_SELECT_PORTRAITS = 8;

//  The button the cursor is on, or nullopt when `screen` is not the language
//  select: every button must read as plain grey but exactly one, blue.
std::optional<size_t> language_select_cursor(const ImageViewRGB32& screen);

//  The portrait the cursor is on, or nullopt when `screen` is not the look
//  select: the background of every portrait must read as plain grey but
//  exactly one, blue.
std::optional<size_t> look_select_cursor(const ImageViewRGB32& screen);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
