/*  AutoStory Start Frame Store
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The start check on frames, and where the start frames are kept.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStartFrameStore_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStartFrameStore_H

#include <cstddef>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "PokemonFRLG_AutoStoryStartFrameCompare.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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

//  The console's start check: how many frames it tries. With
//  CONSOLE_START_FRAME_WAIT between them, they span one period of the fog
//  on Pokemon Tower's upper floors (WEATHER_FOG_HORIZONTAL: 64 pixels, one
//  every 4 GBA frames, 4.3 seconds), so one of them shows the fog within 3
//  pixels of where the kept frame did. Three frames a second apart each
//  differed from it in 52 to 76 of 150 tiles (10.05, 2026-11-12).
constexpr size_t CONSOLE_START_FRAME_TRIES = 12;
//  The wait between two frames of the console's start check.
constexpr std::chrono::milliseconds CONSOLE_START_FRAME_WAIT{400};


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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
