/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Turns what a console's screen detectors saw in one frame (ScreenEvents)
 *  into the look a story backend returns (ScreenObservation).
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenObservation_H
#define PokemonAutomation_Pokemon_ScreenObservation_H

#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The look for one frame's detected `events`. It keeps only the events
//  story steps read; LOCATION_BANNER, BATTLE_ENDED and SWITCH_HOME_MENU are
//  dropped. A choice box sits inside a dialog box, so CHOICE_BOX also sets
//  DIALOG_BOX. The caller fills in the time and the frame.
ScreenObservation screen_observation(const ScreenEvents& events);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
