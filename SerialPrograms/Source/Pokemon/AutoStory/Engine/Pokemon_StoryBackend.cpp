/*  Story Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string_view story_backend_kind_name(StoryBackendKind kind){
    switch (kind){
    case StoryBackendKind::CONSOLE:
        return "console";
    case StoryBackendKind::SIMULATION:
        return "simulation";
    case StoryBackendKind::SCRIPT:
        return "script";
    }
    return "unknown";
}

double direction_degrees(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return 0;
    case Direction::EAST:
        return 90;
    case Direction::SOUTH:
        return 180;
    case Direction::WEST:
        return -90;
    }
    return 0;
}



} // namespace Pokemon
} // namespace PokemonAutomation
