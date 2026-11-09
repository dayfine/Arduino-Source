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


//  What a start reader found about the save just loaded.
enum class StartFound{
    //  It can't tell: the check passes.
    NOT_READ,
    AT_START,
    ELSEWHERE,
};
//  A start reader's answer, with the line the start check logs for it.
struct StartRead{
    StartFound found = StartFound::NOT_READ;
    //  For the log: why nothing was read, or what was seen.
    std::string detail = {};
};

//  The game screen now, as start_frame() makes it; an empty image when
//  there is no video. Called again for each frame tried.
using StartFrameSource = std::function<ImageRGB32()>;

//  The start check on frames: up to `tries` frames from `next_frame`
//  against `reference`, the first one that shows the same place
//  (same_start_place()) passing, so a fade or a passer-by in front of the
//  camera on one frame doesn't fail the check. No reference, or no video,
//  is NOT_READ.
StartRead read_start_frames(const std::optional<ImageRGB32>& reference, const StartFrameSource& next_frame, size_t tries);


//  Where the start frames are kept: for each checkpoint, <id>.png, the
//  frame, and <id>.json, the checkpoint, when it was kept and why.
class StartFrameStore{
public:
    //  Keeps nothing and finds nothing (the simulation).
    StartFrameStore() = default;
    //  `folder` ends with "/".
    explicit StartFrameStore(std::string folder);

    //  Replaces the frame kept for `checkpoint`; `source` says why it is kept.
    //  Returns false if it couldn't be written.
    bool save(const std::string& checkpoint, const ImageViewRGB32& frame, const std::string& source) const;
    //  The frame kept for `checkpoint`, if any.
    std::optional<ImageRGB32> load(const std::string& checkpoint) const;

private:
    std::string m_folder;
};

//  The program's store: USER_FILE_PATH()/PokemonFRLG/AutoStoryStartFrames/.
StartFrameStore program_start_frames();


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
