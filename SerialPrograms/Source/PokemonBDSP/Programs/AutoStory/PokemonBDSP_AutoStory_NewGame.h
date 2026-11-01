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

#include <cstddef>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Which intro screen `screen` shows, as the intro's steps tell them apart:
//  "user-select", "home", "keyboard", "rival-list-tell" (the rival's name list on
//  "I'll tell you..."), "rival-list-preset" (on a preset name), "menu", "dialog" or
//  "other". The Switch's own screens need `console`; without it they read as
//  whatever else they look like.
std::string classify_intro_screen(ConsoleHandle* console, const ImageViewRGB32& screen);

//  What the intro does on a screen. Decided from the screen and how far the intro has
//  got only, so it is tested without a console.
enum class IntroStep{
    SELECT_USER,            //  A on the Switch's user select
    FAIL_NOT_IN_GAME,       //  HOME is showing
    FAIL_NOT_BDSP,          //  no BDSP screen after INTRO_PRESSES_BEFORE_BDSP presses
    ENTER_PLAYER_NAME,      //  type the name on the first keyboard
    CANCEL_RIVAL_KEYBOARD,  //  X: back to the rival's name list
    MOVE_TO_PRESET,         //  Down: off "I'll tell you..."
    PICK_PRESET,            //  A on a preset name
    CLOSE_MENU_AND_FINISH,  //  B: the menu opened in the bedroom, the intro is over
    ADVANCE,                //  A: dialogue, language, appearance, cutscenes
    PROBE_FOR_MENU,         //  X, to see whether the player is in control
};
struct IntroProgress{
    bool player_named = false;
    bool rival_named = false;
    //  A screen only BDSP shows has been seen (intro_screen_confirms_bdsp()).
    bool game_confirmed = false;
    //  A presses on unknown screens before that: the title, the language.
    size_t unconfirmed_presses = 0;
};
//  Unknown screens pressed through before a BDSP screen must show: the title
//  and the language select take a few. Another game launched by mistake
//  (r1, 2026-11-01: FireRed) fails here instead of being played.
inline constexpr size_t INTRO_PRESSES_BEFORE_BDSP = 20;
//  Whether `screen` (classify_intro_screen()) is one only BDSP's intro shows:
//  its dialogue box, the rival's name list, its main menu. The keyboard and
//  the user select are the Switch's.
bool intro_screen_confirms_bdsp(const std::string& screen);
IntroStep decide_intro_step(const std::string& screen, const IntroProgress& progress);

//  After PROBE_FOR_MENU found no menu: whether A may follow. Only when X changed
//  nothing, since A on something X opened would pick from it (the Bag).
bool advance_after_probe(bool screen_changed);

//  Checks classify_intro_screen() on <folder>/<expected name>/*.png at 1920 and
//  1280 px wide. Logs each mismatch; returns the number of them.
size_t run_intro_screen_tests(Logger& logger, const std::string& folder);

//  From the title screen or any intro screen of a game with no save, with BDSP
//  running in front: from HOME it throws UserSetupError, since the restart would
//  launch whatever tile HOME's cursor is on. Restarts the game first, so it
//  always starts from the title, and fails if no BDSP screen shows within
//  INTRO_PRESSES_BEFORE_BDSP presses. Names the player `player_name`
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
