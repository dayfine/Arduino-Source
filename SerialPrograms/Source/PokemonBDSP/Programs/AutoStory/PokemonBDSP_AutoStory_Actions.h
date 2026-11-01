/*  BDSP AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  The game's menus as the story uses them: saving, opening the X menu, and
 *  waiting for a dialogue.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Actions_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Actions_H

#include <chrono>
#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_MenuCursorDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Save the game from the overworld with retries. Returns true if the save was
//  verified via overworld detection, false if the blind fallback was used.
//  With `expected_location`, the map named on the save screen's card must be
//  it (compared as a location banner is), or the save is backed out of and
//  the function fails with a screenshot: r124a saved in Sandgem Town after a
//  walk that was sure it stood on Route 202.
bool save_game(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::string& expected_location = ""
);

//  Opens the save screen from the overworld, reads where the game says we
//  are (read_save_screen_location() in the Location module), and backs out
//  without saving.
std::string read_location_from_save_screen(VideoStream& stream, ProControllerContext& context);

//  Wait (without pressing any buttons) until a short dialogue box appears.
//  Logs `label + " dialogue detected."` when found (omit label to skip logging).
//  Throws OperationFailedException if no dialogue appears within `timeout`.
void wait_for_dialogue(
    VideoStream& stream,
    ProControllerContext& context,
    const std::string& label = "",
    Milliseconds timeout = std::chrono::seconds(60)
);

//  Opens the X-menu from the overworld, navigates the cursor to `target`, and presses A.
//  num_icons: 5 (early-game layout) or 8 (late-game layout).
void open_menu(VideoStream& stream, ProControllerContext& context, MenuCursorPosition target, int num_icons);



}
}
}
#endif
