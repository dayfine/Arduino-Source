/*  PokemonLZA AutoStory - Launch from HOME
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A new game's first step: from the Switch HOME menu, or with the game in
 *  front (HOME once), launch the selected tile without moving HOME's
 *  cursor. Nothing here is PLZA's: the game's first screen comes in as a
 *  detector, so BDSP and FRLG can share it once it moves to the shared
 *  AutoStory core.
 *
 *  Why not upstream's close_game_from_home() / ensure_at_home()
 *  (NintendoSwitch_GameEntry.h): their retry paths press DOWN and HOME on
 *  the HOME menu, which moves HOME's cursor to another tile; a new game
 *  must launch the tile the user selected. An upstream option to keep the
 *  cursor would let this use them.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLaunch_H
#define PokemonAutomation_PokemonLZA_AutoStoryLaunch_H

#include <functional>
#include <string_view>
#include <utility>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A detector for one screen.
using ScreenCheck = std::function<bool(const ImageViewRGB32& frame)>;

//  A ScreenCheck as a detector, for wait_until().
class ScreenCheckDetector : public StaticScreenDetector{
public:
    explicit ScreenCheckDetector(ScreenCheck check)
        : m_check(std::move(check))
    {}
    virtual void make_overlays(VideoOverlaySet&) const override{}
    virtual bool detect(const ImageViewRGB32& screen) override{
        return m_check(screen);
    }

private:
    ScreenCheck m_check;
};

//  Until `check` holds on a frame (wait_until()), or `timeout`. Returns
//  whether it held.
bool wait_for_screen(ConsoleHandle& console, ProControllerContext& context, const ScreenCheck& check, Milliseconds timeout);


//  The screen the launch step reads.
enum class LaunchStart{
    //  The HOME menu.
    HOME_MENU,
    //  The game's first screen (a launch left there). Told apart from OTHER
    //  only for the log.
    FIRST_SCREEN,
    //  Anything else: a game in front, mid-scene or not.
    OTHER,
    //  No frame: no video to check HOME against.
    NO_VIDEO,
};

//  Where the launch step starts, from a frame. `home_menu_shows` is the
//  HOME menu detector's reading of `frame` (that detector needs the
//  console); `first_screen` is the game's first screen's detector.
LaunchStart read_launch_start(const ImageViewRGB32& frame, bool home_menu_shows, const ScreenCheck& first_screen);

//  What the launch step does next.
enum class LaunchMove{
    //  Launch the selected tile.
    LAUNCH,
    //  Press HOME once and read the screen again: with a game in front, HOME
    //  opens with that game's tile selected.
    PRESS_HOME,
    //  Refuse: UserSetupError.
    REFUSE,
};

//  The launch step's move on `start`; `home_pressed` is whether it has
//  pressed HOME already (it presses it once at most).
LaunchMove next_launch_move(LaunchStart start, bool home_pressed);


//  From the HOME menu with `game`'s tile selected, or with `game` in front
//  (on any screen, mid-scene too), a new game (no save): launches the
//  selected tile, never moving HOME's cursor. If HOME is not showing, it
//  presses HOME once: with a game in front, HOME opens with that game's
//  tile selected. It presses no DOWN or UP, and HOME never on the HOME
//  menu: from HOME that goes back to the game in front, and from there
//  DOWN and UP select the first tile, the game played last. If the selected game is running, X on its tile opens the close
//  dialog, which A confirms; on a game that is not running X does nothing.
//  Then A on the tile, the user select and the online license check
//  (start_game_from_home() with slot 0, which does not move the cursor),
//  5 s, and up to `timeout` for `first_screen` (`first_screen_name` in the
//  log), whose frame it saves to DebugDumps under `dump_folder`.
//  Throws UserSetupError if there is no video or HOME still does not show
//  after the HOME press, OperationFailedException
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
