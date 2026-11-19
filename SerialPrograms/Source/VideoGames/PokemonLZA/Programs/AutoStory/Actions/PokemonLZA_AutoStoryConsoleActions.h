/*  PokemonLZA AutoStory - Console actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console's code for the actions that run only on a console (new
 *  game, gates, catches, zone walks, Mable's reward), by action name, for
 *  PlzaConsoleHooks::console.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConsoleActions_H
#define PokemonAutomation_PokemonLZA_AutoStoryConsoleActions_H

#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryActionTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Each catches the OperationFailedException its code throws as a failed
//  step. `env` must outlive them.
PlzaConsoleActions plza_console_actions(StoryEnv& env);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
