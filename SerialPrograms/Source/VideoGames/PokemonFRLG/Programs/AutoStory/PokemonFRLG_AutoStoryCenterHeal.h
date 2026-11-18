/*  AutoStory Center Heal
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's heal at a Pokemon Center's nurse, counted into the run's
 *  supplies.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCenterHeal_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCenterHeal_H

#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The nurse's heal (heal_at_pokecenter()), defined in
//  PokemonFRLG_AutoStoryHealingSteps.cpp, with the party's HP and PP
//  counted full in env.supplies; only where a Pokemon Center's floor shows
//  on screen, else it fails with the screen.
void heal_in_center(StoryEnv& env);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
