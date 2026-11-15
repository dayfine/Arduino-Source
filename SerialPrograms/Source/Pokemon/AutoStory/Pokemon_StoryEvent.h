/*  Story Event
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a checkpoint as the event runner sees it: its name, why it
 *  is there, and its action.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvent_H
#define PokemonAutomation_Pokemon_StoryEvent_H

#include <string>
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One step of a checkpoint: an action, named for the log.
struct Event{
    std::string name;
    std::string why;
    Act act;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
