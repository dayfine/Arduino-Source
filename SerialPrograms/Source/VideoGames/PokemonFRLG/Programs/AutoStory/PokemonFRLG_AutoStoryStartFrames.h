/*  AutoStory Start Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console's start check, with no reader for the map or the tile: a
 *  frame of the game where each checkpoint starts, kept when the program
 *  saves there, and the frame after that save is loaded compared with it.
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
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonFRLG_AutoStoryStartFrameStore.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


ImageRGB32 console_start_frame(ConsoleHandle& console, ProControllerContext& context);

StartRead check_console_start(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store, const std::string& checkpoint
);

bool keep_console_frame(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store,
    const std::string& checkpoint, const std::string& source
);

StartRead keep_console_start(
    ConsoleHandle& console, ProControllerContext& context, const StartFrameStore& store, const std::string& checkpoint
);

using StartReader = std::function<StartRead()>;

//  Runs the start check for a checkpoint with declared `start` pose.
//  Returns std::nullopt when no start pose, when unreadable, or when matched;
//  returns a repeating "start_failed" StoryFailure on mismatch.
std::optional<Pokemon::StoryFailure> run_start_check(
    const std::optional<Pokemon::GridPose>& start,
    const StartReader& read,
    const std::function<void(const std::string& line)>& log
);

void add_tests_AutoStoryStartFrames(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
