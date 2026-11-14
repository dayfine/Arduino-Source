/*  Story Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run's first event: from wherever the console is (the Switch's HOME
 *  menu with the game closed or suspended, the game's title, the
 *  overworld) to the overworld on a save. One driver for every game; the
 *  game's StoryLaunchPolicy says how its screens look and which press
 *  answers each. It runs before the first checkpoint, on its own, never
 *  inside a new game's language or intro steps, and it never starts a new
 *  game.
 *
 *  The driver looks, does one thing, and looks again. It does not need to
 *  know every screen: one it doesn't know is waited out (a boot, a fade)
 *  or, before the game was launched, left with HOME or B. Each screen's
 *  presses are counted, so a screen that ignores its press fails the
 *  launch instead of looping, and the launch ends only once the overworld
 *  shows for several looks in a row.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryLaunch_H
#define PokemonAutomation_Pokemon_StoryLaunch_H

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_ZonePose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a launch loads.
struct StoryLaunchRequest{
    enum class Save{
        LATEST,     //  the title's plain load
        BACKUP,     //  the policy's backup_save presses on the title first
                    //  (PLZA: Up, then B + X, the save before the latest)
    };
    enum class Running{
        //  A game already on the overworld is the start: it is resumed
        //  from HOME, not reloaded.
        KEEP,
        //  A game already running is closed from HOME, then launched, so
        //  the save loads; a title on screen already counts as launched.
        RESTART,
    };
    Save save = Save::LATEST;
    Running running = Running::RESTART;
};

//  One press: `button` (several pressed together when or-ed), held for
//  `hold`, then let go for `release`.
struct StoryLaunchPress{
    NintendoSwitch::Button button = NintendoSwitch::BUTTON_A;
    Milliseconds hold = Milliseconds(80);
    Milliseconds release = Milliseconds(400);
};

//  A screen on the way that one press answers: the user select, an online
//  license check, HOME's close-software dialog, the load menu's Continue,
//  a recap of the story so far.
struct StoryLaunchPrompt{
    std::string name = {};
    std::function<bool(const ScreenObservation& look)> matches = {};
    StoryLaunchPress press = {};
    //  Pressed in order before `press`, each time: the cursor moves to the
    //  choice `press` picks (the update prompt's last choice).
    std::vector<StoryLaunchPress> before = {};
    //  Read only once this launch launched or closed the game: a screen
    //  the game also shows in play (a dialog box, a choice), where a press
    //  before the launch would answer the story instead.
    bool after_launch = false;
    //  Presses it may take before the launch is stuck on it.
    size_t max_presses = 6;
};

//  How one game's screens look on the way from HOME to its overworld.
struct StoryLaunchPolicy{
    //  "frlg", "bdsp", "plza", for the log.
    std::string game = {};
    //  On HOME: whether the cursor is on this game's tile. The launch
    //  presses A on HOME only then, as A launches whatever tile is
    //  selected. Required; on a Switch, switch_home_tile_named() with the
    //  name HOME shows for the game.
    std::function<bool(const ScreenObservation& look)> tile_selected = {};
    //  How far the tile search moves each way: Left `tile_search` times
    //  (the most recently played games are leftmost), then Right twice as
    //  many.
    size_t tile_search = 6;
    //  The title screen, which leads to the save. Required.
    std::function<bool(const ScreenObservation& look)> title = {};
    StoryLaunchPress title_press = {};
    //  `title` is read only once this launch launched or closed the game:
    //  a loose check (any quiet screen after the boot) that a game in play
    //  would pass too.
    bool title_after_launch = false;
    //  Pressed once on the title before `title_press` for Save::BACKUP.
    //  Empty: the game has no backup save, and a BACKUP launch fails
    //  before it presses anything.
    std::vector<StoryLaunchPress> backup_save = {};
    //  A screen that would start a new game: there is no save. The launch
    //  fails there without a press. Optional.
    std::function<bool(const ScreenObservation& look)> new_game = {};
    //  The game's other screens on the way, each answered by its press.
    std::vector<StoryLaunchPrompt> prompts = {};
    //  The player in control. Unset: the look shows PLAYER_CAN_MOVE.
    std::function<bool(const ScreenObservation& look)> overworld = {};
    //  Looks in a row the overworld must show before the launch is done.
    size_t settled_looks = 3;
    //  The wait between looks at a screen with nothing to press.
    Milliseconds poll = Milliseconds(500);
    //  After A on the game's tile, how long HOME may still show (its zoom
    //  into the game, a check or the user select in front of it) before
    //  the launch takes A again: until then each look at HOME waits, as
    //  another A there would launch the game again.
    Milliseconds launch_wait = Milliseconds(30000);
    //  Presses HOME, the title, an overworld to leave or a screen the
    //  policy doesn't know may take before the launch is stuck there.
    size_t presses_per_screen = 6;
    //  Looks before the launch gives up: about `poll` times this of
    //  loading at most.
    size_t max_looks = 360;
};

//  A `tile_selected` for a Switch game: HOME's cursor is on the tile whose
//  name, above the game row, contains `name` ("Pokémon Legends: Z-A"),
//  read in `language`, the console's (NintendoSwitch_HomeTileReader.h).
//  False on a look without a frame, as in a simulation.
std::function<bool(const ScreenObservation& look)> switch_home_tile_named(Language language, std::string name);

//  The failure of a launch that did not reach the overworld; its detail
//  says where it stopped. Nothing in the story answers it: no reload, no
//  recovery.
inline constexpr std::string_view STORY_LAUNCH_FAILED = "launch_failed";

//  Drives `backend` to the overworld on the save `request` asks for, by
//  `policy`; logs each press as "[Launch] <game>: <screen>: <what>".
//  Succeeds once the overworld shows `policy.settled_looks` looks in a
//  row; fails as STORY_LAUNCH_FAILED on a new game, a missing tile, a
//  screen that took too many presses, or after `policy.max_looks` looks.
template <StoryPose PoseType>
Outcome launch_story_game(StoryBackend<PoseType>& backend, const StoryLaunchPolicy& policy, StoryLaunchRequest request);

//  Instantiated once, in Pokemon_StoryLaunch.cpp.
extern template Outcome launch_story_game<GridPose>(StoryBackend<GridPose>&, const StoryLaunchPolicy&, StoryLaunchRequest);
//  And for ZonePose (Pokemon_ZonePose.h).
extern template Outcome launch_story_game<ZonePose>(StoryBackend<ZonePose>&, const StoryLaunchPolicy&, StoryLaunchRequest);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
