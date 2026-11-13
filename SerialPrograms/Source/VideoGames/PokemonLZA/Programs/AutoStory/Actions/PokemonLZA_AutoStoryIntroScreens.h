/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One detector per intro screen. The language list's is measured on
 *  console B (2026-11-13). The name screen is the Switch system keyboard
 *  (NintendoSwitch_SystemKeyboardDetector.h). TODO: the look picker's
 *  returns false until it is built from the frames a run saves
 *  (DebugDumps/PokemonLZA/AutoStory/Intro); its action's
 *  (PokemonLZA_AutoStoryScenes.h) wait is fixed until then.
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

//  The look picker's 2 x 4 grid of faces. TODO: always false until a
//  capture of the screen exists.
bool is_look_picker(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
