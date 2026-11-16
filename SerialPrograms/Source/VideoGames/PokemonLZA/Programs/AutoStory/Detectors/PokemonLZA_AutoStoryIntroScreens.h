/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Readers of the new game's screens before "Confirm Choices" (whose
 *  highlight is in PokemonLZA_AutoStoryConfirmChoicesReader.h): the
 *  language list, the look picker and the Switch's name keyboard, and the
 *  name and language "Confirm Choices" shows. Measured on the console's
 *  frames in ten languages (2026-11-13).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_H
#define PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_H

#include <cstddef>
#include <optional>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On the Switch HOME, whether the highlighted tile is PLZA: the title the
//  HOME prints above the tile row (cyan) ends in "Z-A", read with OCR
//  (English; measured on an English system). False for any other tile or
//  frame.
bool is_home_tile_plza(const ImageViewRGB32& frame);

//  On "Which language will you play in?", the highlighted entry (numbered
//  down the left column, then down the right one): the one white box of
//  the 10, the others slate. Nullopt for any other frame, an empty one
//  included.
std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame);

//  On the look picker ("LOOKS"), the look (1 to 8) whose portrait is
//  highlighted: the one tile whose 4 corners are on the white card. Nullopt
//  while none is (the screen opens so), while the card moves, and for any
//  other frame.
std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame);

//  The look picker's 2 x 4 portraits on dark blue, highlighted or not.
bool is_look_picker(const ImageViewRGB32& frame);

//  The Switch's name keyboard: the dark question area on top, the light
//  key panel below. Any of its layouts (Latin, Hangul, Chinese).
bool is_name_keyboard(const ImageViewRGB32& frame);

//  The name keyboard's character limit, read with OCR from its "0/N"
//  counter right under the text field (it sits further right with a
//  12-character field). Nullopt when no counter reads as one.
std::optional<size_t> read_name_keyboard_limit(const ImageViewRGB32& frame);

//  The player name "Confirm Choices" shows, read with OCR (English), its
//  spaces dropped. Empty when nothing reads.
std::string read_confirm_choices_name(const ImageViewRGB32& frame);

//  The language "Confirm Choices" shows, read with OCR in `language`.
std::string read_confirm_choices_language(const ImageViewRGB32& frame, Language language);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
