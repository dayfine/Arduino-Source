// code-health: allow header_symbols -- story event dispatch table and per-area event population functions
/*  BDSP AutoStory - Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's steps that only BDSP has, by name: each is registered as a
 *  top-level S11 action in the story runner.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryEvents_H
#define PokemonAutomation_PokemonBDSP_AutoStoryEvents_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Runs a step on the console. Returns std::nullopt on success or a
//  StoryFailure; throws OperationFailedException on console errors.
using StoryEvent = std::function<std::optional<Pokemon::StoryFailure>(
    StoryEnv& console, BdspRunState& state, const BdspStep& step
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
