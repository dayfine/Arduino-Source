/*  Story Outcome
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_StoryEvents.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryOutcome_H
#define PokemonAutomation_Pokemon_StoryOutcome_H

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryPose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How an action or a check ended: empty `failure` is success; otherwise a
//  name that recoveries match (e.g. "probe_exhausted"). A succeeded action
//  that knows where it left the player sets `pose`; the runner writes it
//  into the run before the event's expects are checked.
struct Outcome{
    std::string failure;
    std::string detail;
    std::optional<Pose> pose;
    //  A succeeded action that left the game as it found it: a read, a
    //  check, a TM the lead already knows. A reload of the same save undoes
    //  nothing it did.
    bool changed_nothing = false;

    bool succeeded() const;
};
// Constructs a successful event outcome, updating the run pose when given one.
Outcome outcome_ok(std::optional<Pose> pose = std::nullopt);
//  outcome_ok() with `changed_nothing`.
Outcome outcome_unchanged();
// Constructs a failed event outcome with reason and optional diagnostic detail.
Outcome outcome_failed(std::string failure, std::string detail = "");
//  "probe_exhausted: 4 rows": the failure and its detail, if any.
std::string to_string(const Outcome& outcome);

//  The failure of a step that found the Switch's HOME menu instead of the
//  game. Nothing in the story can answer it, as every press goes to the
//  console's menus: no recovery runs and no reload, the run stops.
inline constexpr std::string_view GAME_NOT_RUNNING = "game_not_running";
//  GAME_NOT_RUNNING, saying so for the run log.
Outcome outcome_game_not_running();


// Callable action executed for a story event.
using Act = MoveOnlyFunction<Outcome(RunState& state)>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
