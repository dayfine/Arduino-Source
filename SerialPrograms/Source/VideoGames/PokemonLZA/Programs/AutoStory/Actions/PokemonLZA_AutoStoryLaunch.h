/*  PokemonLZA AutoStory - Launch from HOME
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A new game's first step: from the Switch HOME menu, launch the selected
 *  tile without moving HOME's cursor. Nothing here is PLZA's: the game's
 *  first screen comes in as a detector, so BDSP and FRLG can share it once
 *  it moves to the shared AutoStory core.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLaunch_H
#define PokemonAutomation_PokemonLZA_AutoStoryLaunch_H

#include <functional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A detector for one screen.
using ScreenCheck = std::function<bool(const ImageViewRGB32& frame)>;


//  What the launch step does with the screen it starts on.
enum class LaunchStart{
    //  The HOME menu: launch the selected tile.
    HOME_MENU,
    //  The game's first screen already (a launch left there): refused, as
    //  any other screen. Told apart only for the message.
    FIRST_SCREEN,
    //  Anything else, an empty frame included: refused.
    OTHER,
};

//  Where the launch step starts, from its first frame. `home_menu_shows` is
//  the HOME menu detector's reading of `frame` (that detector needs the
//  console); `first_screen` is the game's first screen's detector.
LaunchStart read_launch_start(const ImageViewRGB32& frame, bool home_menu_shows, const ScreenCheck& first_screen);


//  From the HOME menu with `game`'s tile selected, a new game (no save):
//  launches the selected tile, never moving HOME's cursor (no DOWN, UP or
//  HOME presses: from HOME those go to the first tile, the game played
//  last). If the selected game is running, X on its tile opens the close
//  dialog, which A confirms; on a game that is not running X does nothing.
//  Then A on the tile, the user select and the online license check
//  (start_game_from_home() with slot 0, which does not move the cursor),
//  5 s, and up to `timeout` for `first_screen` (`first_screen_name` in the
//  log), whose frame it saves to DebugDumps under `dump_folder`.
//  Throws UserSetupError if HOME is not showing, OperationFailedException
//  (with a screenshot) if the close dialog does not close or the first
//  screen does not show: then the selected game may not be `game`, or it
//  has a save.
void launch_selected_game_from_home(
    ConsoleHandle& console, ProControllerContext& context,
    std::string_view game, std::string_view first_screen_name,
    const ScreenCheck& first_screen, Milliseconds timeout,
    std::string_view dump_folder
);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
