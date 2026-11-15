/*  Story Checkpoint Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint as the event runner sees it: its events, the interrupts
 *  that can stop one part-way, its start and end checks, and the report of
 *  a run of it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpointTypes_H
#define PokemonAutomation_Pokemon_StoryCheckpointTypes_H

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryEvent.h"

namespace PokemonAutomation{
namespace Pokemon{


// Pose confidence adjustment following an event interruption.
enum class InterruptPose{
    UNCHANGED,  //  Cleared where we stood (a talk-only trainer).
    WITHIN_SET, //  Moved an unknown part of a leg: still within the position we had, no longer exactly where.
    LOST,       //  Anywhere.
};
// Policy defining how an interrupted event should be resumed.
enum class ResumePolicy{
    RETRY_EVENT,    //  Run the event again, from its skip_if.
    RECHECK_EXPECT, //  The action may have finished: done if its expects hold, else retry.
    STOP,           //  Stop the checkpoint.
};

//  Something that can stop an action part-way.
struct Interrupt{
    std::string name;
    Check detect;
    Act handle;
    InterruptPose pose = InterruptPose::UNCHANGED;
    ResumePolicy resume = ResumePolicy::RETRY_EVENT;
};
//  Writes what an interrupt did (`effect`) into `pose`.
void apply_interrupt_pose(InterruptPose effect, Pose& pose);


//  The stretch of story between two saves, as events.
struct Checkpoint{
    std::string id;
    //  Must hold before the first event (the save it starts from).
    std::vector<Condition> start;
    std::vector<Interrupt> interrupts;
    std::vector<Event> events;
    //  Must hold after the last event.
    std::vector<Condition> end;
    //  Saves the game after the end check, if set.
    Act save;
    //  More interrupts than this in one event (wild battles, trainers) is
    //  a loop, not bad luck: the event fails as "too_many_interrupts".
    size_t max_interrupts_per_event = 5;
};


// Result report from executing a checkpoint.
struct CheckpointReport{
    bool completed = false;
    //  1-based number of the event that failed; 0 for the start or end check
    //  or the save, or when nothing failed.
    size_t failed_event = 0;
    std::string failed_name;
    Outcome failure;
    size_t events_run = 0;
    size_t events_skipped = 0;
};

//  Where the runner writes its log lines.
using EventLogger = MoveOnlyFunction<void(const std::string& line)>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
