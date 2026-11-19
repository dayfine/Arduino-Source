/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Readers of the new game's screens before "Confirm Choices": the language
 *  list, the look picker, the name keyboard, and the name and language shown.
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


//  On HOME, whether the highlighted tile's title (OCR, English) ends in "Z-A".
bool is_home_tile_plza(const ImageViewRGB32& frame);

//  On the language list, the highlighted entry (numbered down the left column, then
//  the right); nullopt for any other frame.
std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame);

//  On the look picker, the highlighted look (1-8); nullopt while none is (as it
//  opens), while the card moves, and on any other frame.
std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame);

//  The look picker's 2 x 4 portraits on dark blue, highlighted or not.
bool is_look_picker(const ImageViewRGB32& frame);

//  The Switch's name keyboard, any layout (Latin, Hangul, Chinese).
bool is_name_keyboard(const ImageViewRGB32& frame);

//  The limit N from the "0/N" counter under the text field (OCR); nullopt if none reads.
std::optional<size_t> read_name_keyboard_limit(const ImageViewRGB32& frame);

//  The player name "Confirm Choices" shows (OCR, English), spaces dropped; empty
//  when nothing reads.
std::string read_confirm_choices_name(const ImageViewRGB32& frame);

//  The language "Confirm Choices" shows, read with OCR in `language`.
std::string read_confirm_choices_language(const ImageViewRGB32& frame, Language language);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
