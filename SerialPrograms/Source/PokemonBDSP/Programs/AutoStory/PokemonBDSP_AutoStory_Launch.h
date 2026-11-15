/*  BDSP AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How BDSP's screens look on the way from HOME to the overworld, for the
 *  shared launch (Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Launch_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Launch_H

#include <string_view>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The reading a launch look carries when title_screen_shown()
//  (Detect/PokemonBDSP_AutoStory_TitleDetector.h) read the title on it.
inline constexpr std::string_view TITLE_READING = "bdsp.title";

//  BDSP: the tile is "Pokémon Brilliant Diamond" or "Pokémon Shining
//  Pearl" on HOME (Pokemon::switch_home_tile_named()); `switch_prompts`
//  answer the Switch's own screens (Pokemon::switch_launch_prompts() on a
//  console). After the launch, a look with TITLE_READING is the title and
//  takes ZL, as reset_game_from_home() mashed it; the title loads the save
//  straight into the overworld, with no load menu or recap. The boot's
//  logos and the title's attract movie read as the overworld: they take
//  ZL too, and the overworld counts only after ZL on the title and the
//  load's black (StoryLaunchPolicy::overworld_after_load). ZL does nothing
//  on the overworld, so one pressed as the load ends is harmless.
Pokemon::StoryLaunchPolicy bdsp_launch_policy(std::vector<Pokemon::StoryLaunchPrompt> switch_prompts);

//  The game closed from HOME and launched again onto its save, by the
//  shared launch on `console`. Fails as Pokemon::STORY_LAUNCH_FAILED.
Pokemon::Outcome launch_bdsp_save(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
