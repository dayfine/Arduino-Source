/*  BDSP AutoStory - New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays a new game's intro (language, Professor Rowan, appearance, names) up to
 *  the bedroom with the player in control, which is where the Tutorial starts.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_NewGame_H
#define PokemonAutomation_PokemonBDSP_AutoStory_NewGame_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Which intro screen `screen` shows, as the intro's steps tell them apart:
//  "user-select", "home", "keyboard", "rival-list-tell" (the rival's name list on
//  "I'll tell you..."), "rival-list-preset" (on a preset name), "menu", "dialog" or
//  "black" (the boot, fades) or "other". The Switch's own screens need
//  `console`; without it they read as whatever else they look like.
std::string classify_intro_screen(ConsoleHandle* console, const ImageViewRGB32& screen);

//  What the intro does on a screen. Decided from the screen and how far the intro has
//  got only, so it is tested without a console.
enum class IntroStep{
    SELECT_USER,            //  A on the Switch's user select
    FAIL_NOT_IN_GAME,       //  HOME is showing
    FAIL_NOT_BDSP,          //  no BDSP screen within INTRO_TIME_BEFORE_BDSP
    ENTER_PLAYER_NAME,      //  type the name on the first keyboard
    CANCEL_RIVAL_KEYBOARD,  //  X: back to the rival's name list
    MOVE_TO_PRESET,         //  Down: off "I'll tell you..."
    PICK_PRESET,            //  A on a preset name
    CLOSE_MENU_AND_FINISH,  //  B: the menu opened in the bedroom, the intro is over
    ADVANCE,                //  A: dialogue, language, appearance, cutscenes
    ADVANCE_WITH_B,         //  B: dialogue once the rival is named
    CLEAR_WITH_B,           //  B: an unknown screen once the rival is named
    PROBE_FOR_MENU,         //  X, to see whether the player is in control
    WAIT,                   //  nothing: a black screen (the boot, a fade)
};
struct IntroProgress{
    bool player_named = false;
    //  A preset name was picked from the rival's list, and how many screens
    //  have shown since without the list coming back. "Barry, is it?" and
    //  its Yes/No follow, and A (Yes, where the cursor starts) confirms it;
    //  B would answer No and bring the list back (r4, 2026-11-01).
    bool rival_picked = false;
    size_t screens_after_pick = 0;
    //  The name is final: INTRO_SCREENS_AFTER_RIVAL_PICK screens passed
    //  without the list.
    bool rival_named = false;
    //  A screen only BDSP shows has been seen (intro_screen_confirms_bdsp()).
    bool game_confirmed = false;
    //  When the first screen that is not black showed, since the intro
    //  started; none yet while the game boots.
    std::optional<std::chrono::milliseconds> first_shown = std::nullopt;
    //  How long screens have shown without one only BDSP shows: the logos,
    //  the title, the language select.
    std::chrono::milliseconds unconfirmed_for = std::chrono::milliseconds(0);
    //  B presses on unknown screens since the last X.
    size_t b_presses = 0;
};
//  How long the intro presses A on unknown screens, from the first screen
//  that is not black, before a BDSP screen must show: the logos, the title
//  and the language select. Another game launched by mistake (r1,
//  2026-11-01: FireRed) fails here instead of being played. Black screens
//  take no presses: BDSP boots on black for about 25 s (r3, 2026-11-01).
inline constexpr std::chrono::milliseconds INTRO_TIME_BEFORE_BDSP = std::chrono::seconds(60);
//  How long the boot may stay black after the restart.
inline constexpr std::chrono::milliseconds INTRO_BOOT_LIMIT = std::chrono::seconds(60);
//  Whether `screen` (classify_intro_screen()) is one only BDSP's intro shows:
//  its dialogue box, the rival's name list, its main menu. The keyboard and
//  the user select are the Switch's.
bool intro_screen_confirms_bdsp(const std::string& screen);

//  Screens after the rival's name is picked, each taking A, before the name
//  counts as confirmed and B takes over: "Barry, is it?", the box closing
//  (Barry alone), the Yes/No, and a few of Rowan's lines after it.
inline constexpr size_t INTRO_SCREENS_AFTER_RIVAL_PICK = 8;

//  Once the rival is named, the intro only has dialogue and cutscenes left
//  before the bedroom, and screens the classifier misses (r2: a dialogue box
//  without its arrow). Each unknown screen takes B, which advances dialogue,
//  never starts a talk and does nothing in the bedroom; after this many, X
//  checks for the menu, and only the menu ends the intro.
inline constexpr size_t INTRO_B_BEFORE_X = 5;
//  X presses that opened no menu before the intro gives up: with
//  INTRO_B_BEFORE_X B presses between them, about two minutes.
inline constexpr size_t INTRO_X_WITHOUT_MENU = 20;

//  Notes `screen`, read `since_start` into the intro, in `progress`: the
//  game is confirmed on a BDSP screen; black screens leave the clock alone.
void note_intro_screen(IntroProgress& progress, const std::string& screen, std::chrono::milliseconds since_start);
IntroStep decide_intro_step(const std::string& screen, const IntroProgress& progress);

//  When X opens nothing the menu detector knows, the player is still in
//  control if the screen held still for INTRO_STILL_TIME before X (no
//  dialogue, no fade, no intro sparkles), X changed the menu's area by more
//  than INTRO_X_CHANGE, and B put it back within INTRO_B_RESTORE: the menu
//  opened and closed. Changes are pixel RMSD.
inline constexpr std::chrono::milliseconds INTRO_STILL_TIME = std::chrono::seconds(2);
inline constexpr double INTRO_STILL_CHANGE = 4;
inline constexpr double INTRO_X_CHANGE = 25;
inline constexpr double INTRO_B_RESTORE = 10;
struct XProbe{
    double still_change;    //  over INTRO_STILL_TIME before X
    double x_change;        //  from before X to after it
    double b_change;        //  from before X to after the B that follows
};
bool intro_in_control_without_menu(const XProbe& probe);
//  Counts `step` in `progress`.
void record_intro_step(IntroProgress& progress, IntroStep step);

//  Checks classify_intro_screen() on <folder>/<expected name>/*.png at 1920 and
//  1280 px wide. Logs each mismatch; returns the number of them.
size_t run_intro_screen_tests(Logger& logger, const std::string& folder);

//  From the title screen or any intro screen of a game with no save, with BDSP
//  running in front: from HOME it throws UserSetupError, since the restart would
//  launch whatever tile HOME's cursor is on. Restarts the game first, so it
//  always starts from the title, waits out the black boot (up to
//  INTRO_BOOT_LIMIT) without pressing anything, and fails if no BDSP screen shows within
//  INTRO_TIME_BEFORE_BDSP of screens. Names the player `player_name`
//  and picks the rival's default name (Barry) from the list. Each step reads the
//  screen and waits for the expected next one; an unexpected screen fails with a
//  screenshot and the recent screens.
void play_new_game_intro(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const std::string& player_name
);


}
}
}
#endif
