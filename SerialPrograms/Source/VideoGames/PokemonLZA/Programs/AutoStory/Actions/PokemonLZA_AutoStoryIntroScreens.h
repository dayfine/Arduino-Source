/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One detector per intro screen, measured on console B (2026-11-13). The
 *  name screen is the Switch system keyboard
 *  (NintendoSwitch_SystemKeyboardDetector.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_H
#define PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On the "Which language will you play in?" list, the highlighted entry
//  (numbered as language_list_index() numbers them): the one white box of
//  the 10, the others slate. Nullopt for any other frame, an empty one
//  included.
std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame);

//  The language list, with one entry highlighted.
bool is_language_list(const ImageViewRGB32& frame);

//  On the "APPEARANCE" look picker, the look (1 to LOOK_COUNT) whose face is
//  highlighted: the one face whose tile or the strip above it is brighter
//  than the others' slate tiles. Nullopt while no face is highlighted (the
//  screen opens so), and for any other frame.
std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame);

//  The look picker's 2 x 4 grid of slate face tiles on dark blue, with at
//  most one face highlighted.
bool is_look_picker(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
