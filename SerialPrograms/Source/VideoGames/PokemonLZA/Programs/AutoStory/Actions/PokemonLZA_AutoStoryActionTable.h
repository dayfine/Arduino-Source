/*  PokemonLZA AutoStory - Action table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registry binding PLZA plan action names to pure handlers.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActionTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryActionTable_H

#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Returns the action parser table for PLZA plans.
Pokemon::StoryActions plza_story_actions();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
