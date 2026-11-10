/*  Story Checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryCheckLevel.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string story_check_level_name(StoryCheckLevel level){
    switch (level){
    case StoryCheckLevel::THOROUGH:
        return "thorough";
    case StoryCheckLevel::FAST:
        return "fast";
    }
    return "?";
}



} // namespace Pokemon
} // namespace PokemonAutomation
