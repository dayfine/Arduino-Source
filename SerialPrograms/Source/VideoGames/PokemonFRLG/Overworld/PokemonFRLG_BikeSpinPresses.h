/*  Bicycle and Spinner Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The presses the walker sends on the Bicycle and after a slide, from
 *  PokemonFRLG_BikeSpinMechanics.h.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BikeSpinPresses_H
#define PokemonAutomation_PokemonFRLG_BikeSpinPresses_H

#include "Common/Cpp/Time.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_MovementMechanics.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace MovementMechanics{


//  An open leg on the Bicycle is counted out in taps, each a tile: a 40ms
//  press each.
inline constexpr Milliseconds BIKE_TAP(40);

//  The taps' period, start to start, slower than the chains that worked,
//  so none is dropped.
inline constexpr Milliseconds BIKE_TAP_PERIOD(120);

//  After a tap, how long until the screen shows whether it moved: the
//  tile's 100ms and video lag. The Command File Runner's tap check judged
//  its first tap too early, as unmoved. Not measured: a guess.
inline constexpr Milliseconds BIKE_TAP_SETTLE(250);

//  SELECT, pressed to get on the registered Bicycle.
inline constexpr Milliseconds BIKE_SELECT_PRESS(160);

//  The wait after SELECT for the player to be up on the Bicycle.
inline constexpr Milliseconds BIKE_SELECT_SETTLE(1200);

//  The wait after a slide's last tile before the next press: the spin's
//  last frames and the facing it leaves. Not measured.
inline constexpr Milliseconds SPIN_STOP_SETTLE(300);

static_assert(BIKE_TAP_PERIOD > BIKE_TAP_PERIOD_WORKED && BIKE_TAP_PERIOD_WORKED > BIKE_TAP_DROP_PERIOD);
static_assert(BIKE_TAP < BIKE_TILE_TIME && BIKE_TAP <= LONGEST_TAP);
static_assert(BIKE_SELECT_SETTLE > BIKE_SELECT_TIME);


} // namespace MovementMechanics
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
