/*  Bicycle and Spinner Mechanics
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How FireRed and LeafGreen move the player on the Bicycle and on spinner
 *  tiles, as measured on the console (PokemonFRLG_MovementMechanics.h has
 *  walking and running).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BikeSpinMechanics_H
#define PokemonAutomation_PokemonFRLG_BikeSpinMechanics_H

#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace MovementMechanics{


//  Riding the Bicycle, measured on console B (2026-11-09, Lavender Town,
//  east from the rock west of the Center, from rest, counted back with
//  one-tile taps): holds of 450, 500, 650, 700, 900, 950, 1100, 1250,
//  1300, 1450 and 1500ms moved 5, 5, 7, 7, 9, 10, 11, 13, 13, 15 and 15
//  tiles, the same each time: a tile per 100ms, so an open leg's hold of
//  (n - 1) tiles and a half moves n. 112ms a tile (a wall-to-wall time
//  of the motion on screen) held "west 16 (open)" for 17 or 18 tiles on
//  Route 8 (08.03 run 2). Not the decompilation's 4 frames a tile (67ms):
//  the bike speeds up from rest.
inline constexpr Milliseconds BIKE_TILE_TIME(100);

//  On the Bicycle a tap never only turns the player: from rest, a 40ms tap
//  moved a tile facing either way, and holds of 112, 150, 200, 250, 300,
//  336 and 400ms moved 1, 2, 2, 3, 3, 4 and 4 tiles (console B,
//  2026-11-09, Route 12's north pier). So a bike leg has no turn tap: its
//  hold turns the player too, and loses no tile for it.
inline constexpr bool BIKE_TAP_ONLY_TURNS = false;

//  Chains of 40ms taps on the Bicycle (console B, 2026-11-09): one tap per
//  100ms (60ms release) moved exactly a tile each.
inline constexpr Milliseconds BIKE_TAP_PERIOD_WORKED(100);

//  At one tap per 60ms, one tap in 12 was dropped. Somewhere between that
//  and BIKE_TAP_PERIOD_WORKED the game starts dropping taps; the sim drops
//  taps that start sooner than this after the last one.
inline constexpr Milliseconds BIKE_TAP_DROP_PERIOD(80);

//  Of the taps that come sooner than BIKE_TAP_DROP_PERIOD, the sim drops
//  one in this many.
inline constexpr int BIKE_FAST_TAP_DROP_EVERY = 12;

//  With the Bicycle registered (Key Items, BICYCLE, REGISTER), SELECT gets
//  on and off in about 1s (console B, 2026-11-09). Going through a door
//  takes the player off (not measured); a battle leaves them on.
inline constexpr Milliseconds BIKE_SELECT_TIME(1000);

//  A spinner tile (Rocket Hideout, Viridian Gym) slides the player a tile
//  per 8 frames (pokefirered PlayerGoSpin(): MOVE_SPEED_FAST_1). Not
//  measured on the console.
inline constexpr Milliseconds SPIN_TILE_TIME(133);


} // namespace MovementMechanics
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
