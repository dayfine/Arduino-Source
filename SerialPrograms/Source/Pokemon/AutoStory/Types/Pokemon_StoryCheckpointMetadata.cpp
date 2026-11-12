/*  Story Checkpoint Metadata
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryCheckpointMetadata.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string story_phase_name(StoryPhase phase){
    switch (phase){
    case StoryPhase::TESTING:
        return "testing";
    case StoryPhase::VERIFICATION:
        return "verification";
    case StoryPhase::REPRODUCTION:
        return "reproduction";
    }
    return "?";
}

std::optional<StoryPhase> read_story_phase(std::string_view name){
    for (StoryPhase phase : {StoryPhase::TESTING, StoryPhase::VERIFICATION, StoryPhase::REPRODUCTION}){
        if (story_phase_name(phase) == name){
            return phase;
        }
    }
    return std::nullopt;
}



} // namespace Pokemon
} // namespace PokemonAutomation
