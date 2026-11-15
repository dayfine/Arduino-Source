/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_ScreenObservation.h"

namespace PokemonAutomation{
namespace Pokemon{


ScreenObservation screen_observation(const ScreenEvents& events){
    //  The events the story engine reads; the rest are left out.
    constexpr ScreenEvent ENGINE_EVENTS[] = {
        ScreenEvent::FADE_BLACK,
        ScreenEvent::FADE_WHITE,
        ScreenEvent::DIALOG_BOX,
        ScreenEvent::BATTLE_STARTED,
        ScreenEvent::PLAYER_CAN_MOVE,
        ScreenEvent::SCREEN_STILL,
        ScreenEvent::CHOICE_BOX,
        ScreenEvent::INTERACT_PROMPT,
    };
    ScreenObservation look;
    for (ScreenEvent event : ENGINE_EVENTS){
        if (events.has(event)){
            look.events.set(event, true);
        }
    }
    if (events.has(ScreenEvent::CHOICE_BOX)){
        look.events.set(ScreenEvent::DIALOG_BOX, true);
    }
    return look;
}



} // namespace Pokemon
} // namespace PokemonAutomation
