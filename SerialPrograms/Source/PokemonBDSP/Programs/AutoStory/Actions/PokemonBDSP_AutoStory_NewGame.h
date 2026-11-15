/*  BDSP AutoStory - New Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays a new game's intro (language, Professor Rowan, appearance, names) up to
 *  the bedroom with the player in control, which is where the Tutorial starts.
 *  The language and the player's look are picked from their menus by index
 *  (NintendoSwitch_MenuCursor.h); the menus' shapes are the data below.
 *  Both menus are read by their detector (PokemonBDSP_AutoStory_IntroMenuDetector.h).
 *
 */
// code-health: allow header_symbols -- the intro's entry points (play, capture), its two menus' data and its screen classifier

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_NewGame_H
#define PokemonAutomation_PokemonBDSP_AutoStory_NewGame_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Programs/MenuCursor/NintendoSwitch_MenuCursor.h"
#include "PokemonBDSP_AutoStory_IntroSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The language select: two columns of five buttons, English, Espanol,
//  Francais, Deutsch, Italiano on the left; Japanese in kana, Japanese in
//  kanji, Korean, Simplified and Traditional Chinese on the right (lab-B
//  2026-11-09). Items are numbered row by row, as MenuGrid reads them.
inline constexpr MenuGrid BDSP_LANGUAGE_GRID{2, 10};
//  The language of each item of BDSP_LANGUAGE_GRID. Japanese is there twice;
//  its first item, the kana, is the one picked.
const std::vector<Language>& bdsp_language_menu();
//  The look select ("Choose your photo."): two rows of four portraits, the
//  cursor on the first (lab-B 2026-11-09). PlayerModelOption's "Model N" is
//  item N - 1, row by row: Lucas light, Lucas dark, Dawn dark-haired, Dawn
//  dark; Lucas blond, Lucas brown, Dawn blond, Dawn brown-haired. A on it
//  goes straight to the name keyboard.
inline constexpr MenuGrid BDSP_LOOK_GRID{4, 8};

//  What the intro picks on its menus.
struct IntroPicks{
    Language game_language = Language::English;
    //  An item of BDSP_LOOK_GRID.
    size_t look = 0;
};
//  The presses from the language select's item `cursor` to the game's
//  language; nullopt when the menu lacks it or `cursor` is outside it.
std::optional<std::vector<DpadPosition>> language_pick_moves(size_t cursor, const IntroPicks& picks);
//  The presses from the look select's item `cursor` to the chosen look;
//  nullopt when either is outside the grid.
std::optional<std::vector<DpadPosition>> look_pick_moves(size_t cursor, const IntroPicks& picks);


//  Which intro screen `screen` shows, as the intro's steps tell them apart:
//  "user-select", "home", "keyboard", "language-select", "look-select", "rival-list-tell" (the rival's name list on
//  "I'll tell you..."), "rival-list-preset" (on a preset name), "menu", "dialog" or
//  "black" (the boot, fades) or "other". The Switch's own screens need
//  `console`; without it they read as whatever else they look like.
std::string classify_intro_screen(ConsoleHandle* console, const ImageViewRGB32& screen);

namespace detail{
//  Short names of BDSP_LANGUAGE_GRID's items, in its order, for capture
//  mode's file names: "en", "ja-kana", "es", "ja", ...
const std::vector<std::string_view>& bdsp_language_tags();
//  The progress a new intro starts from: which of `picks` need a menu.
IntroProgress intro_start(const IntroPicks& picks);
//  Whether the intro restarts the game when `screen` (classify_intro_screen())
//  shows at its start: not on a screen of the intro itself, which it takes up
//  from there.
bool intro_restart_needed(std::string_view screen);
//  The progress an intro taken up on `screen` starts from: a pick whose
//  screen is past is not made again, and one that may be past (from a
//  dialogue, the look) is made only if its screen still shows. From a
//  keyboard it types the player's name: it cannot tell the rival's.
IntroProgress intro_resume(const IntroPicks& picks, std::string_view screen);
//  Why `name` cannot be typed on a keyboard that takes `limit` characters,
//  or nullopt when it fits or the limit was not read.
std::optional<std::string> name_limit_problem(std::string_view name, std::optional<size_t> limit);
} // namespace detail

//  From the title screen or any intro screen of a game with no save, with BDSP
//  running in front: from HOME it throws UserSetupError, since the restart would
//  launch whatever tile HOME's cursor is on, as it does for picks no menu has.
//  On a screen of the intro (intro_restart_needed()) it takes the intro up
//  from there, without closing the game. Otherwise it restarts the game, so it
//  starts from the title, waits out the black boot (up to
//  INTRO_BOOT_LIMIT) without pressing anything, and fails if no BDSP screen shows within
//  INTRO_TIME_BEFORE_BDSP of screens. Picks the language and the look in
//  `picks`, names the player `player_name` (failing at the name screen,
//  nothing typed, when the keyboard takes fewer characters) and picks the rival's default name
//  (Barry) from the list. The language and look selects are read, moved on
//  and read again before their A. The player's keyboard showing with a pick
//  still to make fails. Each step reads the
//  screen and waits for the expected next one; an unexpected screen fails with a
//  screenshot and the recent screens.
void play_new_game_intro(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const IntroPicks& picks,
    const std::string& player_name
);

//  Developer: shoots the intro's screens for each language of the language
//  select, for a game with no save running in front. Per language it
//  restarts the game, picks the language and `look` (an item of
//  BDSP_LOOK_GRID), and saves the name keyboard that follows as
//  <Screenshots>/<tag>_keyboard.png (bdsp_language_tags()); the first round
//  also saves <tag>_language-list.png and <tag>_look-picker.png before
//  their picks. Leaves the game on the last language's keyboard.
void capture_intro_screens(SingleSwitchProgramEnvironment& env, ProControllerContext& context, size_t look);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
