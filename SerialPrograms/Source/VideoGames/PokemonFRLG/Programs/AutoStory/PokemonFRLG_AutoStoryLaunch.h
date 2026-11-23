/*  AutoStory Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's launch policy for the shared launch
 *  (Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h): the FireRed HOME
 *  tile, the optional online license check, the GBA boot and title, the
 *  load menu's CONTINUE, the "Previously on your quest" recap and the
 *  quiet overworld.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLaunch_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLaunch_H

#include <optional>
#include <string_view>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The readings FRLG's launch backend puts in ScreenObservation::readings.
namespace FrlgLaunchReadings{
    inline constexpr std::string_view CHECK_ONLINE = "frlg.check_online";
    inline constexpr std::string_view LOAD_MENU = "frlg.load_menu";
} // namespace FrlgLaunchReadings

//  The settings FRLG's launch policy takes from the program.
struct FrlgLaunchSettings{
    //  FRLG's HANDLE_ONLINE_LICENSE_CHECK setting.
    bool handle_online_license_check = false;
};

//  FRLG's launch policy, with `switch_prompts` (from
//  Pokemon::switch_launch_prompts() on the console, or scripted in tests).
Pokemon::StoryLaunchPolicy frlg_launch_policy(
    std::vector<Pokemon::StoryLaunchPrompt> switch_prompts,
    const FrlgLaunchSettings& settings = {}
);

//  Closes the game from HOME (or wherever it is), launches FireRed, chooses
//  CONTINUE and clears the recap to the overworld. Returns std::nullopt on
//  success, else StoryFailure.
std::optional<Pokemon::StoryFailure> launch_frlg_save(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
