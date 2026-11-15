/*  Story Segment Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The plain data of a segment run (Engine/Pokemon_StoryEngine.h): the
 *  report of what happened.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySegmentTypes_H
#define PokemonAutomation_Pokemon_StorySegmentTypes_H

#include <cstddef>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


// Summary of checkpoint attempts and pass status.
struct CheckpointRun{
    std::string id;
    bool passed = false;
    size_t attempts = 0;
};

// Execution results and failure diagnostics for a story segment run.
struct SegmentReport{
    bool completed = false;
    //  Both empty when nothing failed; the event is empty when a save or a
    //  reload failed.
    std::string failed_checkpoint;
    std::string failed_event;
    Outcome failure;
    size_t checkpoints_run = 0;
    //  Reloads after failed attempts.
    size_t reloads = 0;
    //  Each checkpoint run, in order, the failed one last.
    std::vector<CheckpointRun> runs;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
