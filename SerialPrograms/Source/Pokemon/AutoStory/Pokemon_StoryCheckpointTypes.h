/*  Story Checkpoint Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint as the event runner sees it: its events, and the report of
 *  a run of it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpointTypes_H
#define PokemonAutomation_Pokemon_StoryCheckpointTypes_H

#include <cstddef>
#include <string>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryEvent.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The stretch of story between two saves, as events.
struct Checkpoint{
    std::string id;
    std::vector<Event> events;
};

// Result report from executing a checkpoint.
struct CheckpointReport{
    bool completed = false;
    //  1-based number of the event that failed; 0 when nothing failed.
    size_t failed_event = 0;
    std::string failed_name;
    Outcome failure;
    size_t events_run = 0;
};

//  Where the runner writes its log lines.
using EventLogger = MoveOnlyFunction<void(const std::string& line)>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
