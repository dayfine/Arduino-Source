/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story checkpoint as a list of events, run straight through: each
 *  event's action in order, until one fails, which stops the checkpoint
 *  with the event and the reason.
 *
 *  The run state is the runner's to change. An action that knows where it
 *  left the player says so in its Outcome, and the runner writes that into
 *  the pose once the action has succeeded.
 *
 *  Actuation is a callable the game supplies, so this file knows nothing
 *  of any one game or console, and the runner can be tested against a
 *  scripted world.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvents_H
#define PokemonAutomation_Pokemon_StoryEvents_H

#include <cstddef>
#include <string>
#include <string_view>
#include "Pokemon_StoryCheckpointTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  "[Event] 00.04/2 to_lab: start": event `number` (from 1) of
//  `checkpoint`, named `event`, and its status.
std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status);

//  "[Checkpoint] 00.04: done, 3 run".
std::string checkpoint_log_line(std::string_view checkpoint, std::string_view text);

//  Runs `checkpoint`'s events in order from `state`, logging each one's
//  start and end, until one fails. An exception from an action is logged
//  as the event's failure and goes on: the game decides what it means.
CheckpointReport run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
