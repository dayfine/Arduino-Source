/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Bridges a ScreenEventObserver (Pokemon::ScreenEvents) to the story
 *  engine's look (ScreenObservation).
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenObservation_H
#define PokemonAutomation_Pokemon_ScreenObservation_H

#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A ScreenEventObserver's events as one look for the story engine:
//  DIALOG_BOX is TEXT_BOX; CHOICE_BOX adds TEXT_BOX; BATTLE_STARTED is
//  BATTLE; PLAYER_CAN_MOVE is CAN_MOVE; SCREEN_STILL is STILL; the fades
//  and INTERACT_PROMPT keep their names. No time and no frame.
ScreenObservation screen_observation(const ScreenEvents& events);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
