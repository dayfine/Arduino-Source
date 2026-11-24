/*  AutoStory Switch Launch Prompts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Switch's own screens between HOME and a game, each answered by one
 *  press: HOME's "Close software?", the user select and the update prompt.
 *  They are the same in every game, so each game's StoryLaunchPolicy
 *  (Engine/Pokemon_StoryLaunch.h) starts its prompts with these.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySwitchPrompts_H
#define PokemonAutomation_Pokemon_StorySwitchPrompts_H

#include <vector>
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace Pokemon{


//  On `console`'s frames: "Close software?" takes A, then the console's
//  close delay (Switch settings); the user select takes A on the user
//  highlighted; the update prompt takes Up twice, to "Start Software", then
//  A. A look without a frame (a simulation) shows none of them.
std::vector<StoryLaunchPrompt> switch_launch_prompts(NintendoSwitch::ConsoleHandle& console);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
