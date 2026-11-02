/*  BDSP AutoStory - Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's steps that only BDSP has, by name: a step of the table whose
 *  variant names one of these runs it (StoryExecutor::run()). Some are the
 *  scripted scenes (the intro's briefcase, the starter, the Starly battle,
 *  the Pokedex); most are segment 01's walks, which were measured on the
 *  console as holds, taps and walks until stopped rather than planned, and
 *  are kept as they ran there. Each fails by throwing, with a screenshot;
 *  the executor turns that into a failed step.
 *
 *  They are in files by area: _Events_Lake (00.00 to 00.03), _Sandgem
 *  (00.04), _Jubilife (00.05, 01.01, 01.06 to 01.08), _Oreburgh (01.02 to
 *  01.05) and _Route204 (01.09 to 01.11).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Events_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Events_H

#include <functional>
#include <map>
#include <string>
#include "PokemonBDSP_AutoStory_StoryExecutor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Runs a step on the console. Returns where it left the player when it
//  knows; throws OperationFailedException when it fails.
using StoryEvent = std::function<Pokemon::Outcome(
    StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep& step
)>;
using StoryEvents = std::map<std::string, StoryEvent>;

//  Every event of the story, by name.
const StoryEvents& story_events();

//  An event that only acts and says nothing of where it leaves us.
StoryEvent console_event(std::function<void(VideoStream& stream, ProControllerContext& context)> action);

void add_lake_events(StoryEvents& events);
void add_sandgem_events(StoryEvents& events);
void add_jubilife_events(StoryEvents& events);
void add_route203_events(StoryEvents& events);
void add_oreburgh_events(StoryEvents& events);
void add_route204_events(StoryEvents& events);



}
}
}
#endif
