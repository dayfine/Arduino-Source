/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story checkpoint as a list of events. Each event states what must hold
 *  before it (require), does one thing (act), and checks what the game shows
 *  afterwards (expect). An event whose effect is already on screen (skip_if)
 *  is skipped, so resuming a checkpoint after a reload re-checks instead of
 *  replaying. A failed event is answered by its named recoveries, in order
 *  of escalation (wait, then re-anchor, then stop), and anything without a
 *  recovery stops the checkpoint with the event and the reason.
 *
 *  Interrupts (a wild battle, a trainer, a scene) are declared once per
 *  checkpoint: after each action the runner asks whether one is showing,
 *  clears it, marks what it did to the pose and resumes as it says.
 *
 *  The run state is the runner's to change. An action that knows where it
 *  left the player says so in its Outcome, and the runner writes that into
 *  the pose once the action has succeeded.
 *
 *  Observation and actuation are callables the game supplies, so this file
 *  knows nothing of any one game or console, and the runner can be tested
 *  against a scripted world. See docs/design/AutoStory-Event-Model.md.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvents_H
#define PokemonAutomation_Pokemon_StoryEvents_H

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryCheckpointTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Problems an author should fix before running: unnamed or empty events,
//  missing actions, recoveries out of escalation order.
std::vector<std::string> lint_checkpoint(const Checkpoint& checkpoint);

//  A readable dump for review, one line per item: the checkpoint's start,
//  interrupts and end by name, each event with its why, its conditions'
//  names, its learned pose and its recoveries. Nothing runs.
std::string describe(const Checkpoint& checkpoint);

//  Runs the checkpoint's events in order, from the first. Events whose
//  effect is already showing are skipped, so after a reload this resumes.
//  Logs each event's start and end through `log`.
CheckpointReport run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
