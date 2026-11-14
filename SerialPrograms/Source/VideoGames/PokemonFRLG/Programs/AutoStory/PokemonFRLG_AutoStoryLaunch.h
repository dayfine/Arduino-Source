/*  AutoStory Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How FRLG's screens look on the way from HOME to the overworld, for the
 *  shared launch (Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h): the
 *  game is "Pokémon FireRed Version" on HOME, a GBA game the Switch runs,
 *  whose title leads to the load menu's CONTINUE.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLaunch_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLaunch_H

#include <string_view>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The readings (ScreenObservation::readings, 1 when shown) FRLG's launch
//  reads: the console fills them from its detectors, a test sets them.
struct FrlgLaunchReadings{
    //  The load menu: CONTINUE, NEW GAME, OPTION (LoadMenuDetector).
    static constexpr std::string_view LOAD_MENU = "frlg.load_menu";
    //  HOME's "Close software?".
    static constexpr std::string_view CLOSE_SOFTWARE = "switch.close_software";
    static constexpr std::string_view USER_SELECT = "switch.user_select";
    static constexpr std::string_view UPDATE = "switch.update";
    //  "Checking if the software can be played...", the online license
    //  check.
    static constexpr std::string_view CHECK_ONLINE = "switch.check_online";
};

//  What FRLG's launch takes from the program's settings.
struct FrlgLaunchSettings{
    //  Game Settings' Handle Online License Check: the console checks the
    //  license online at each launch, and HOME shows for a few seconds
    //  after the tile's A, each look there answered with A again.
    bool handle_online_license_check = false;
    //  The Switch settings' Close Game Delay, after A on "Close software?".
    Milliseconds close_delay = Milliseconds(0);
};

//  FRLG: the Switch's prompts (Close software, the user select, the
//  update, the license check) take A, Up Up A for the update; after the
//  launch, any quiet screen before the load menu is the GBA's boot or its
//  title and takes A; the load menu takes A, CONTINUE; after it, a dialog
//  box (the "Previously on your quest" recap) takes B, and a quiet screen
//  is the overworld. No HOME tile reader yet: the tile under the cursor is
//  taken as FireRed's, as the RNG programs' reset does.
Pokemon::StoryLaunchPolicy frlg_launch_policy(const FrlgLaunchSettings& settings);

//  The game closed from HOME and launched again onto its save, by the
//  shared launch on `console`, with the program's Game Settings; then B
//  until the START menu opens on the overworld (mash_until_overworld()).
//  Fails as Pokemon::STORY_LAUNCH_FAILED.
Pokemon::Outcome launch_frlg_save(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
