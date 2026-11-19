/*  Story Checkpoint Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint as events, and the result of running it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpointTypes_H
#define PokemonAutomation_Pokemon_StoryCheckpointTypes_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon_StoryEvent.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The stretch of story between two saves, as events.
struct Checkpoint{
    std::string id = {};
    std::vector<Event> events = {};
};

//  Where and why a checkpoint run stopped.
struct CheckpointFailure{
    //  0-based; none when the checkpoint failed outside its events (a lint
    //  or a reload).
    std::optional<size_t> event_index = std::nullopt;
    Outcome outcome = {};
};

//  How a checkpoint run ended.
struct CheckpointRunResult{
    //  None: every event passed.
    std::optional<CheckpointFailure> failure = std::nullopt;
    //  The run is on a save made at the checkpoint's end.
    bool saved = false;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
