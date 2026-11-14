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
#include <vector>
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
};

//  FRLG: the tile is "Pokémon FireRed Version" on HOME
//  (Pokemon::switch_home_tile_named()); `switch_prompts` answer the
//  Switch's own screens (Pokemon::switch_launch_prompts() on a console),
//  and with the license check handled, its screen takes A; after the
//  launch, any quiet screen before the load menu is the GBA's boot or its
//  title and takes A; the load menu takes A, CONTINUE; after it, a dialog
//  box (the "Previously on your quest" recap) takes B, and a quiet screen
//  is the overworld.
Pokemon::StoryLaunchPolicy frlg_launch_policy(
    std::vector<Pokemon::StoryLaunchPrompt> switch_prompts,
    const FrlgLaunchSettings& settings
);

//  The game closed from HOME and launched again onto its save, by the
//  shared launch on `console`, with the program's Game Settings; then B
//  until the START menu opens on the overworld (mash_until_overworld()).
//  Fails as Pokemon::STORY_LAUNCH_FAILED.
Pokemon::Outcome launch_frlg_save(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
