/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Bridges a ScreenEventObserver (ScreenEvents) to the story
 *  engine's look (ScreenObservation).
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenObservation_H
#define PokemonAutomation_Pokemon_ScreenObservation_H

#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A ScreenEventObserver's events as one look for the story engine: the
//  events the engine reads (not LOCATION_BANNER, BATTLE_ENDED or
//  SWITCH_HOME_MENU), and CHOICE_BOX adds DIALOG_BOX. No time and no frame.
ScreenObservation screen_observation(const ScreenEvents& events);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
