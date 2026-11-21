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


//  Whether `look`, read after the launch, counts as PLZA's title: a frame
//  that is not a fade, the overworld (the party bar) or a battle. An A
//  prompt alone counts: the title's centred "PRESS (A) BUTTON" reads as one
//  (console B, 2026-11-14), and the overworld's prompts come with the
//  party bar.
bool plza_title(const ScreenObservation& look);

//  PLZA on `console`: the Switch's prompts, then the game's own, as
//  reset_game_from_home() answered them: the backup save's arrow takes A,
//  a dialog box B. The title (plza_title()) takes A; the backup save is Up,
//  B and X held together on it. No HOME tile reader yet: the tile
//  under the cursor is taken as PLZA's, as reset_game_from_home() did.
Pokemon::StoryLaunchPolicy plza_launch_policy(ConsoleHandle& console);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
