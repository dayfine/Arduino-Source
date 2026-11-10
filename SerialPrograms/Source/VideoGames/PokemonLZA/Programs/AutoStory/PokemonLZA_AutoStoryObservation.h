/*  PokemonLZA AutoStory - Observations
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's screen events (PokemonLZA_AutoStoryScreenEvents.h) as the story
 *  engine's observations, which its backend returns and its scene states
 *  match on.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryObservation_H
#define PokemonAutomation_PokemonLZA_AutoStoryObservation_H

#include <optional>
#include <string_view>
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEventTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PlzaScreenObserver's events as one look for the story engine:
//  DIALOG_BOX is TEXT_BOX; CHOICE_BOX adds TEXT_BOX;
//  BATTLE_STARTED is BATTLE; PLAYER_CAN_MOVE (the party bar) is CAN_MOVE;
//  SCREEN_STILL is STILL; the fades and INTERACT_PROMPT keep their names.
//  No time and no frame: the backend adds them.
ScreenObservation plza_observation(const Pokemon::ScreenEvents& events);

//  The engine's event for a fade of the shared events; none for another
//  event.
std::optional<PokemonAutomation::ScreenEvent> engine_fade_event(Pokemon::ScreenEvent event);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
