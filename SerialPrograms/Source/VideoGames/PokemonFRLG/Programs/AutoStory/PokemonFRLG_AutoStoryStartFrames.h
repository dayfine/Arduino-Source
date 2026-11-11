/*  AutoStory Start Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console's start check, with no reader for the map or the tile: a
 *  frame of the game where each checkpoint starts, kept when the program
 *  saves there, and the frame after that save is loaded compared with it.
 *  FRLG's camera follows the player on whole tiles, so the same pose shows
 *  the same tiles, up to people walking about and animated tiles (water,
 *  flowers); another place, even a step away, shows other tiles.
 *
 *  Also the start check itself (add_start_check()), which takes its
 *  reader: frames on the console, the save's pose in the simulation.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStartFrames_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStartFrames_H

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
#include "PokemonFRLG_AutoStoryStartFrameStore.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  The game screen on the console now, as start_frame() makes it; empty
//  with no video.
ImageRGB32 console_start_frame(ConsoleHandle& console, ProControllerContext& context);

//  The console's start check for `checkpoint`: the screen against the
//  frame `store` keeps for it, up to 3 frames a second apart
//  (read_start_frames()).
StartRead check_console_start(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store, const std::string& checkpoint
);

//  The screen kept as `checkpoint`'s start frame, `source` saying why.
//  Returns false if there was no video or it couldn't be written.
bool keep_console_frame(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store,
    const std::string& checkpoint, const std::string& source
);

//  The start check for a save made by hand: the screen is kept as
//  `checkpoint`'s start frame, and the save is taken to be there.
StartRead keep_console_start(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store, const std::string& checkpoint
);


//  Where the save just loaded is, against the start of the checkpoint it
//  starts.
using StartReader = std::function<StartRead()>;

//  Adds to `checkpoint`, built from a row that starts at `start`, the start
//  check: the save it starts from, as `read` finds it, must be there, or
//  the checkpoint stops before its first event, "start_failed". A save
//  `read` can't tell about passes. `log` gets a mismatch ("start pose
//  mismatch: "), that nothing was read ("start pose not read: "), or what
//  a match saw. A row with no start (a new game) gets none.
void add_start_check(
    Pokemon::Checkpoint& checkpoint, const std::optional<Pokemon::Pose>& start, StartReader read,
    std::function<void(const std::string& line)> log
);


//  Registers the comparison's and the store's unit tests (synthetic frames).
void add_tests_AutoStoryStartFrames(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
