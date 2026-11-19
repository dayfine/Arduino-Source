/*  Story Event
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a checkpoint, as the event runner sees it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvent_H
#define PokemonAutomation_Pokemon_StoryEvent_H

#include <string>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What an event does; it may move the run's pose.
using Act = MoveOnlyFunction<Outcome(RunState& state)>;

//  One step of a checkpoint: an action, named for the log.
struct Event{
    std::string name = {};
    std::string why = {};
    Act act = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
