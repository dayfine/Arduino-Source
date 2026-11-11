/*  AutoStory Console Preflight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story run's preflight (StoryPreflight) read off a Switch console:
 *  the same for every game, so BDSP and FRLG lend the same reader.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryConsolePreflight_H
#define PokemonAutomation_Pokemon_StoryConsolePreflight_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon_StoryRun.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Whether `frame` shows a picture: not empty, not all black, and not the
//  capture card's color bars (no signal from the Switch).
bool story_frame_shows_picture(const ImageViewRGB32& frame);

//  Reads the console: a frame newer than two seconds that shows a picture,
//  the controller ready, and that frame not the HOME menu. Presses nothing.
StoryPreflight read_story_preflight(NintendoSwitch::ConsoleHandle& console, NintendoSwitch::ProControllerContext& context);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
