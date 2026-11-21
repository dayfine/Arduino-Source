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

std::optional<int> story_heading(const ScreenObservation& observation){
    const auto reading = observation.readings.find(STORY_HEADING);
    if (reading == observation.readings.end()){
        return std::nullopt;
    }
    return reading->second;
}



} // namespace Pokemon
} // namespace PokemonAutomation
