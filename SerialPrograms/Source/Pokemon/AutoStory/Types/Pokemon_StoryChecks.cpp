/*  Story Checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryChecks.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* story_checks_name(StoryChecks checks){
    switch (checks){
    case StoryChecks::THOROUGH:
        return "thorough";
    case StoryChecks::FAST:
        return "fast";
    }
    return "?";
}



} // namespace Pokemon
} // namespace PokemonAutomation
