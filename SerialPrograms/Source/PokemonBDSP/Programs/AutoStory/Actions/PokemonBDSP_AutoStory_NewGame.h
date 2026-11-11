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


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
