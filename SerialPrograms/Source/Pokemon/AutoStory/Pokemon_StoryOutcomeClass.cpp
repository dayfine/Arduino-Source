/*  Story Outcome Class
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryOutcomeClass.h"

namespace PokemonAutomation{
namespace Pokemon{


std::optional<StoryFailureClass> failure_class_of(const Outcome& outcome){
    if (outcome.succeeded()){
        return std::nullopt;
    }
    if (outcome.failure_class){
        return outcome.failure_class;
    }
    return story_failure_class_of_outcome(outcome.failure);
}

std::string to_string_with_class(const Outcome& outcome){
    const std::optional<StoryFailureClass> failure_class = failure_class_of(outcome);
    if (!failure_class){
        return to_string(outcome);
    }
    return std::format("{} ({})", to_string(outcome), story_failure_class_tag(*failure_class));
}



} // namespace Pokemon
} // namespace PokemonAutomation
