/*  BDSP AutoStory - Intro steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the intro does on each screen, decided from the screen and how far
 *  the intro has got only, so it is tested without a console.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_IntroSteps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_IntroSteps_H

#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What the intro does on a screen. Decided from the screen and how far the intro has
//  got only, so it is tested without a console.
enum class IntroStep{
    SELECT_USER,            //  A on the Switch's user select
    FAIL_NOT_IN_GAME,       //  HOME is showing
    FAIL_NOT_BDSP,          //  no BDSP screen within INTRO_TIME_BEFORE_BDSP
    FAIL_PICK_MISSED,       //  the player's keyboard, with a pick still to make
    PICK_LANGUAGE,          //  the language select: move to the game's language, A
    PICK_LOOK,              //  the look select: move to the chosen look, A
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
//  Progress tracking state for the new-game introductory sequence.
struct IntroProgress{
    //  Picks still to make on the language select and the appearance grid.
    //  The first look needs no menu: A takes it.
    bool language_pending = false;
    bool look_pending = false;
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
//  Whether `screen` (classify_intro_screen()) is one only BDSP's intro shows:
//  its dialogue box, its language and look selects, the rival's name list, its main menu. The keyboard and
//  the user select are the Switch's.
bool intro_screen_confirms_bdsp(std::string_view screen);

//  Decides the next controller action based on the current screen and intro progress.
IntroStep decide_intro_step(std::string_view screen, const IntroProgress& progress);

namespace detail{
//  How long the intro presses A on unknown screens, from the first screen
//  that is not black, before a BDSP screen must show: the logos, the title
//  and the language select. Another game launched by mistake (r1,
//  2026-11-01: FireRed) fails here instead of being played. Black screens
//  take no presses: BDSP boots on black for about 25 s (r3, 2026-11-01).
inline constexpr std::chrono::milliseconds INTRO_TIME_BEFORE_BDSP = std::chrono::seconds(60);
//  How long the boot may stay black after the restart.
inline constexpr std::chrono::milliseconds INTRO_BOOT_LIMIT = std::chrono::seconds(60);
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
//  Counts `step` in `progress`.
void record_intro_step(IntroProgress& progress, IntroStep step);
//  Notes `screen`, read `since_start` into the intro, in `progress`: the
//  game is confirmed on a BDSP screen; black screens leave the clock alone.
void note_intro_screen(IntroProgress& progress, std::string_view screen, std::chrono::milliseconds since_start);
} // namespace detail


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
