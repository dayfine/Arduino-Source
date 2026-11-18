/*  AutoStory Start Frame Compare
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint start's frame as the start check keeps it, and how another
 *  frame differs from it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStartFrameCompare_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStartFrameCompare_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The game screen as the start check keeps and compares it: the GBA's own
//  240x160 pixels, 15x10 tiles of 16. `game_screen` is the game's part of
//  the frame (GameSettings' GAME_BOX).
ImageRGB32 start_frame(const ImageViewRGB32& game_screen);

//  How a frame differs from a checkpoint start's frame, tile by tile.
struct StartFrameDifference{
    size_t tiles = 0;
    //  The tiles whose pixels differ by more than an animated tile's do.
    size_t changed = 0;
};

//  The two frames, each scaled as start_frame() does, tile by tile.
StartFrameDifference compare_start_frames(const ImageViewRGB32& reference, const ImageViewRGB32& frame);

//  Whether the difference leaves the same place: few enough tiles changed
//  for people who moved, too many for another place (see the .cpp).
bool same_start_place(const StartFrameDifference& difference);

//  "12 of 150 tiles changed".
std::string to_string(const StartFrameDifference& difference);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
