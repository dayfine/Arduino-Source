// code-health: allow header_symbols -- story event dispatch table and per-area event population functions
/*  BDSP AutoStory - Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's steps that only BDSP has, by name: a step of the table whose
 *  variant names one of these runs it (StoryExecutor::run()). Some are the
 *  scripted scenes (the Azure Flute, the lake's briefcase, the starter,
 *  Dawn's catching tutorial and the catch); the rest do what the shared
 *  steps cannot, as they ran on the console: the Poke Mart lady's Potions
 *  and the stairs into Jubilife (00.04c, 00.04d), a clown who wanders
 *  (00.05b), the Gym's tries from the Pokemon Center (01.04) and Rock Smash
 *  (01.09). Each fails by throwing, with a screenshot; the executor turns
 *  that into a failed step.
 *
 *  They are in files by area: _Events_Lake (00.00 to 00.04), _Sandgem
 *  (00.04c, 00.04d), _Jubilife (00.05b), _Oreburgh (01.04) and _Route204
 *  (01.09).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Events_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Events_H

#include <functional>
#include <map>
#include <string>
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Runs a step on the console. Returns where it left the player when it
//  knows; throws OperationFailedException when it fails.
using StoryEvent = std::function<Pokemon::Outcome(
    StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep& step
)>;
//  Map of story event names to their execution functions.
using StoryEvents = std::map<std::string, StoryEvent>;

//  Every event of the story, by name.
const StoryEvents& story_events();

//  An event that only acts and says nothing of where it leaves us.
StoryEvent console_event(std::function<void(VideoStream& stream, ProControllerContext& context)> action);

//  Populates lake-area story event handlers.
void add_lake_events(StoryEvents& events);
//  Populates Sandgem-area story event handlers.
void add_sandgem_events(StoryEvents& events);
//  Populates Jubilife-area story event handlers.
void add_jubilife_events(StoryEvents& events);
//  Populates Oreburgh-area story event handlers.
void add_oreburgh_events(StoryEvents& events);
//  Populates Route 204-area story event handlers.
void add_route204_events(StoryEvents& events);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
