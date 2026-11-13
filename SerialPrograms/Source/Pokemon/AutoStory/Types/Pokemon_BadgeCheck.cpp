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
    if (reading == BadgeReading::PRESENT){
        return "";
    }
    if (reading == BadgeReading::UNREADABLE){
        return std::format(
            "The Trainer Card could not be read, so the {} is not confirmed (the card's screenshot is kept).",
            badge
        );
    }
    return std::format(
        "The Trainer Card has no {}: the battle that ended the step was not the gym leader's.",
        badge
    );
}



} // namespace Pokemon
} // namespace PokemonAutomation
