/*  Story Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
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

ScreenObservation screen_observation(const ScreenEvents& events){
    using EngineEvent = PokemonAutomation::ScreenEvent;
    ScreenObservation look;
    const std::pair<ScreenEvent, EngineEvent> same[] = {
        {ScreenEvent::FADE_BLACK, EngineEvent::FADE_BLACK},
        {ScreenEvent::FADE_WHITE, EngineEvent::FADE_WHITE},
        {ScreenEvent::DIALOG_BOX, EngineEvent::TEXT_BOX},
        {ScreenEvent::CHOICE_BOX, EngineEvent::TEXT_BOX},
        {ScreenEvent::BATTLE_STARTED, EngineEvent::BATTLE},
        {ScreenEvent::PLAYER_CAN_MOVE, EngineEvent::CAN_MOVE},
        {ScreenEvent::SCREEN_STILL, EngineEvent::STILL},
        {ScreenEvent::CHOICE_BOX, EngineEvent::CHOICE_BOX},
        {ScreenEvent::INTERACT_PROMPT, EngineEvent::INTERACT_PROMPT},
    };
    for (const auto& [event, engine_event] : same){
        if (events.has(event)){
            look.events.add(engine_event);
        }
    }
    return look;
}



} // namespace Pokemon
} // namespace PokemonAutomation
