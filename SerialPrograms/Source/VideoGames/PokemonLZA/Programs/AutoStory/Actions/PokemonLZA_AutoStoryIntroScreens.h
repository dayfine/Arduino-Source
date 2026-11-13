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
#include <vector>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonLZA_AutoStoryIntro.h"

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
//  highlighted: the one tile whose 4 corners are on the white card while
//  the other 7 tiles' corners are slate. Nullopt while no face is
//  highlighted (the screen opens so), while the card moves between looks,
//  and for any other frame.
std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame);

//  The look picker's 2 x 4 grid of face tiles on dark blue, each tile's
//  corners white (the highlight's card) or slate.
bool is_look_picker(const ImageViewRGB32& frame);

//  "Confirm Choices", the screen after the name keyboard, has three rows:
//  "Change hair color" (0, highlighted as it opens), "Change look" and
//  "Ready!", the last.
constexpr size_t CONFIRM_CHOICES_READY_ROW = 2;

//  The presses on "Confirm Choices" from the highlighted row `cursor` to
//  "Ready!": DOWN to it, then A. Nullopt for a row past "Ready!".
std::optional<std::vector<IntroKey>> confirm_choices_moves(size_t cursor);

//  On "Confirm Choices", the PROFILE screen after the name keyboard (its
//  rows "Change hair color", "Change look" and "Ready!"), the highlighted
//  row, 0 to CONFIRM_CHOICES_READY_ROW: the one white box of the three, the
//  others slate. Nullopt for any other frame, an empty one included.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame);

//  "Confirm Choices", with one row highlighted.
bool is_confirm_choices(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
