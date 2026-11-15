/*  Story Outcome
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


bool Outcome::succeeded() const{
    return failure.empty();
}

Outcome outcome_ok(std::optional<Pose> pose){
    Outcome outcome;
    outcome.pose = std::move(pose);
    return outcome;
}

Outcome outcome_unchanged(){
    Outcome outcome;
    outcome.changed_nothing = true;
    return outcome;
}

Outcome outcome_failed(std::string failure, std::string detail, std::optional<StoryFailureClass> failure_class){
    Outcome outcome;
    outcome.failure = std::move(failure);
    outcome.detail = std::move(detail);
    outcome.failure_class = failure_class;
    return outcome;
}

std::string to_string(const Outcome& outcome){
    return outcome.detail.empty() ? outcome.failure : std::format("{}: {}", outcome.failure, outcome.detail);
}

Outcome outcome_game_not_running(){
    return outcome_failed(std::string(GAME_NOT_RUNNING), "the game is not running: the Switch HOME menu is showing");
}



} // namespace Pokemon
} // namespace PokemonAutomation
