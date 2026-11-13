/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One detector per intro screen, for the actions' fixed waits
 *  (PokemonLZA_AutoStoryScenes.h) to end early once they are written. TODO:
 *  each returns false until a detector is built from the frames a run saves
 *  (DebugDumps/PokemonLZA/AutoStory/Intro).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_H
#define PokemonAutomation_PokemonLZA_AutoStoryIntroScreens_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The "Which language will you play in?" list.
bool is_language_list(const ImageViewRGB32& frame);

//  The look picker's grid of faces.
bool is_look_picker(const ImageViewRGB32& frame);

//  The Switch keyboard on the name screen.
bool is_keyboard(const ImageViewRGB32& frame);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
