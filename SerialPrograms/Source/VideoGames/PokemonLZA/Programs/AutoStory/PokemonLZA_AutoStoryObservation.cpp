/*  PokemonLZA AutoStory - Observations
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <string>
#include <utility>
#include "PokemonLZA_AutoStoryObservation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;


ScreenObservation plza_observation(const ScreenEvents& events){
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

std::optional<PokemonAutomation::ScreenEvent> engine_fade_event(ScreenEvent event){
    switch (event){
    case ScreenEvent::FADE_BLACK:
        return PokemonAutomation::ScreenEvent::FADE_BLACK;
    case ScreenEvent::FADE_WHITE:
        return PokemonAutomation::ScreenEvent::FADE_WHITE;
    default:
        return std::nullopt;
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
