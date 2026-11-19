/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs a checkpoint's events in order until one fails.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvents_H
#define PokemonAutomation_Pokemon_StoryEvents_H

#include <cstddef>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryCheckpointTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where run_checkpoint() writes its log lines.
using EventLogger = MoveOnlyFunction<void(std::string_view line)>;

//  "[Event] 00.04/2 to_lab: start": event `number` (from 1) of
//  `checkpoint`, named `event`, and its status.
std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status);

//  "[Checkpoint] 00.04: done, 3 run".
std::string checkpoint_log_line(std::string_view checkpoint, std::string_view text);

//  Runs `checkpoint`'s events in order from `state` until one fails; a
//  succeeded event's pose becomes the run's pose. Exceptions pass through.
CheckpointRunResult run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
