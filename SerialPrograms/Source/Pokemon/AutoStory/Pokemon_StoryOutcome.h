/*  Story Outcome
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a story action or check ended: success, or a named failure with its
 *  class, and the pose the action left the player in when it knows it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryOutcome_H
#define PokemonAutomation_Pokemon_StoryOutcome_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_StoryFailureClasses.h"
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
    //  The failure's class, when the action knows it: a game's own failure
    //  name ("probe_exhausted") has none in the shared list until the game
    //  gives it one here. Nothing: the class the list gives `failure`.
    std::optional<StoryFailureClass> failure_class = std::nullopt;

    bool succeeded() const;
};
//  Success; with a pose, the runner moves the run's pose to it.
Outcome outcome_ok(std::optional<Pose> pose = std::nullopt);
//  outcome_ok() with `changed_nothing`.
Outcome outcome_unchanged();
//  A failure named `failure`, with what was seen in `detail`.
//  `failure_class` is for a name the shared list does not class (a game's
//  own); without it the list's class for `failure` holds.
Outcome outcome_failed(
    std::string failure, std::string detail = "",
    std::optional<StoryFailureClass> failure_class = std::nullopt
);
//  "probe_exhausted: 4 rows": the failure and its detail, if any.
std::string to_string(const Outcome& outcome);

//  The failure of a step that found the Switch's HOME menu instead of the
//  game. Nothing in the story can answer it, as every press goes to the
//  console's menus: no recovery runs and no reload, the run stops.
inline constexpr std::string_view GAME_NOT_RUNNING = "game_not_running";
//  GAME_NOT_RUNNING, saying so for the run log.
Outcome outcome_game_not_running();


//  What an event does; it may move the run's pose.
using Act = MoveOnlyFunction<Outcome(RunState& state)>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
