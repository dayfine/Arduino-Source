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

#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  BDSP: the tile is "Pokémon Brilliant Diamond" or "Pokémon Shining
//  Pearl" on HOME (Pokemon::switch_home_tile_named()); `switch_prompts`
//  answer the Switch's own screens (Pokemon::switch_launch_prompts() on a
//  console). After the launch, a quiet screen (no fade, box, battle or
//  overworld) is the boot's logos or the title and takes ZL, as
//  reset_game_from_home() mashed it: ZL does nothing on the overworld, so
//  one pressed as the load ends is harmless. The title loads the save
//  straight into the overworld, with no load menu or recap.
Pokemon::StoryLaunchPolicy bdsp_launch_policy(std::vector<Pokemon::StoryLaunchPrompt> switch_prompts);

//  The game closed from HOME and launched again onto its save, by the
//  shared launch on `console`. Fails as Pokemon::STORY_LAUNCH_FAILED.
Pokemon::Outcome launch_bdsp_save(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
