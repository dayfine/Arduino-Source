/*  PokemonLZA AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How PLZA's screens look on the way from HOME to the overworld, for the
 *  shared launch (Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLaunch_H
#define PokemonAutomation_PokemonLZA_AutoStoryLaunch_H

#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PLZA on `console`: the Switch's prompts, then the game's own, as
//  reset_game_from_home() answered them: the backup save's arrow takes A,
//  a dialog box B. The title is any screen after the launch that is not a
//  fade, a prompt or the overworld, and takes A; the backup save is Up,
//  then B + X held for a second, on it. No HOME tile reader yet: the tile
//  under the cursor is taken as PLZA's, as reset_game_from_home() did.
Pokemon::StoryLaunchPolicy plza_launch_policy(ConsoleHandle& console);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
