/*  Badge Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_BadgeCheck.h"

namespace PokemonAutomation{
namespace Pokemon{


bool badge_check_wanted(StoryCheckLevel level){
    return level == StoryCheckLevel::TESTING || level == StoryCheckLevel::THOROUGH;
}

std::string badge_check_failure(BadgeReading reading, std::string_view badge){
    if (reading != BadgeReading::MISSING){
        return "";
    }
    return std::format(
        "The Trainer Card has no {}: the battle that ended the step was not the gym leader's.",
        badge
    );
}



} // namespace Pokemon
} // namespace PokemonAutomation
