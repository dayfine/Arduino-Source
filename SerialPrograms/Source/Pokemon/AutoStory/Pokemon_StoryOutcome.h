/*  Story Outcome
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a story action or check ended.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryOutcome_H
#define PokemonAutomation_Pokemon_StoryOutcome_H

#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  How an action or a check ended.
struct Outcome{
    //  Empty: success; otherwise a name that recoveries match.
    std::string failure = {};
    std::string detail = {};
    //  A succeeded action that left the game as it found it (a read, a check).
    bool changed_nothing = false;

    bool succeeded() const;
};
//  Success.
Outcome outcome_ok();
//  outcome_ok() with `changed_nothing`.
Outcome outcome_unchanged();
//  A failure named `failure`, with what was seen in `detail`.
Outcome outcome_failed(std::string failure, std::string detail = "");
//  "probe_exhausted: 4 rows": the failure and its detail, if any.
std::string to_string(const Outcome& outcome);

//  The failure of a step that found the Switch's HOME menu instead of the
//  game: the run stops, with no recovery and no reload.
inline constexpr std::string_view GAME_NOT_RUNNING = "game_not_running";
//  GAME_NOT_RUNNING, saying so for the run log.
Outcome outcome_game_not_running();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
