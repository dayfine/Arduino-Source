/*  Story Event
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_StoryEvents.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvent_H
#define PokemonAutomation_Pokemon_StoryEvent_H

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


// Predicate callable evaluating current run state.
using Check = MoveOnlyFunction<bool(RunState& state)>;

//  A question the screen (or the run state) answers, named for the log.
struct Condition{
    std::string name;
    Check check;
};
// Constructs a named Condition from a predicate check.
Condition condition(std::string name, Check check);

//  Holds while the pose is on `map` (any map if empty) and at least as
//  certain as `need`.
Condition pose_known(std::string map, PoseCertainty need);


// Strategy for recovering from a failed event.
enum class RecoveryKind{
    WAIT,       //  Something transient (a wanderer, a slow scene): wait, try again.
    REANCHOR,   //  The pose is in doubt: confirm it on a landmark, try again.
    STOP,       //  Nothing to do: stop the checkpoint.
};
// Returns the readable name of a recovery strategy.
std::string recovery_name(RecoveryKind kind);

//  Answers a failure named `on` (any failure if empty): runs `prepare`, if
//  any, and tries the event again, up to `attempts` times.
struct Recovery{
    std::string on;
    RecoveryKind kind = RecoveryKind::STOP;
    Act prepare;
    size_t attempts = 1;
    std::string why;
};


// Single executable story action with preconditions, recovery steps, and postconditions.
struct Event{
    std::string name;
    std::string why;
    //  Skipped when all of these hold (and there is at least one).
    std::vector<Condition> skip_if;
    //  Must hold before `act`; a failure is "require_failed".
    std::vector<Condition> require;
    Act act;
    //  Must hold after `act`; a failure is "expect_failed".
    std::vector<Condition> expect;
    //  Written to the run's pose once the event succeeds. An action that
    //  learns its pose as it goes (the row that answered) returns it in
    //  its Outcome instead.
    std::optional<Pose> pose_learned;
    //  In order of escalation.
    std::vector<Recovery> recoveries;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
