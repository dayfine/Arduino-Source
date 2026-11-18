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



} // namespace Pokemon
} // namespace PokemonAutomation
