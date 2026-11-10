/*  Movement Mechanics
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How FireRed and LeafGreen move the player for a press of the D-pad, as
 *  measured on the console, and the presses the walker sends because of
 *  it. The overworld walker (OverworldWalkSteps, AutoStory's step_once())
 *  presses by this table, and OverworldSim moves its player by the same
 *  rules.
 *
 *  Taps measured on console B (2026-11-09, Route 10's Pokemon Center 1F,
 *  D-pad presses with a 700ms release, tiles counted by exact frame match
 *  against reference shots), 140 of 140 alike:
 *  -   From a standstill, a tap of 16 to 133ms while the player faces
 *      another way only turns them, never steps (pokefirered turns in 8
 *      frames).
 *  -   The same tap while they face its way takes exactly one step.
 *  -   No tap was dropped.
 *  Repeated step taps (66ms each, after a turn tap and a 1000ms wait):
 *  -   A step tap that starts within STEP_TAP_LOCKOUT of the previous step
 *      tap's start is ignored.
 *  -   Taps with no release between them are one hold: after a turn, 3 of
 *      them took 0 or 1 steps.
 *  After a battle (console B, 2026-11-09 15:50, 00.28 winston_line): a
 *  step tap sent as flee_battle() returned, on the black's end, was lost;
 *  one more tap by hand stepped.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MovementMechanics_H
#define PokemonAutomation_PokemonFRLG_MovementMechanics_H

#include <cstddef>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace MovementMechanics{


//  The game.

//  Walking: a tile per 16 frames at 60 fps (console B, 2026-11-09).
inline constexpr Milliseconds TILE_TIME(267);
//  Running with B held: a tile per 8 frames (console B, 2026-11-02, video
//  of run 00.11).
inline constexpr Milliseconds RUN_TILE_TIME(133);
//  Surfing: a tile per 8 frames.
inline constexpr Milliseconds SURF_TILE_TIME(133);
//  Riding the Bicycle, measured on console B (2026-11-09, Lavender Town,
//  east from the rock west of the Center, from rest, counted back with
//  one-tile taps): holds of 450, 500, 650, 700, 900, 950, 1100, 1250,
//  1300, 1450 and 1500ms moved 5, 5, 7, 7, 9, 10, 11, 13, 13, 15 and 15
//  tiles, the same each time: a tile per 100ms, so an open leg's hold of
//  (n - 1) tiles and a half moves n. 112ms a tile (a wall-to-wall time
//  of the motion on screen) held "west 16 (open)" for 17 or 18 tiles on
//  Route 8 (00.30 run 2). Not the decompilation's 4 frames a tile (67ms):
//  the bike speeds up from rest.
inline constexpr Milliseconds BIKE_TILE_TIME(100);
//  On the Bicycle a tap never only turns the player: from rest, a 40ms tap
//  moved a tile facing either way, and holds of 112, 150, 200, 250, 300,
//  336 and 400ms moved 1, 2, 2, 3, 3, 4 and 4 tiles (console B,
//  2026-11-09, Route 12's north pier). So a bike leg has no turn tap: its
//  hold turns the player too, and loses no tile for it.
inline constexpr bool BIKE_TAP_ONLY_TURNS = false;
//  Chains of 40ms taps on the Bicycle (console B, 2026-11-09): one tap per
//  100ms (60ms release) moved exactly a tile each; at one per 60ms, one
//  tap in 12 was dropped. Somewhere between the two the game starts
//  dropping taps; the sim drops one in BIKE_FAST_TAP_DROP_EVERY of the taps
//  that start sooner than this after the last one.
inline constexpr Milliseconds BIKE_TAP_PERIOD_WORKED(100);
inline constexpr Milliseconds BIKE_TAP_DROP_PERIOD(80);
inline constexpr int BIKE_FAST_TAP_DROP_EVERY = 12;
//  With the Bicycle registered (Key Items, BICYCLE, REGISTER), SELECT gets
//  on and off in about 1s (console B, 2026-11-09). Going through a door
//  takes the player off (not measured); a battle leaves them on.
inline constexpr Milliseconds BIKE_SELECT_TIME(1000);
//  The longest tap measured to only turn a player facing another way, and
//  to take exactly one step for one facing its way.
inline constexpr Milliseconds LONGEST_TAP(133);
//  A step tap that starts sooner than this after the previous step tap
//  started is ignored. Measured between 566ms (66ms taps 500ms apart: the
//  2nd of 3 ignored, 3 of 3 times) and 666ms (600 to 900ms apart: 8 of 8
//  worked).
inline constexpr Milliseconds STEP_TAP_LOCKOUT(620);
//  The shortest step tap spacing, start to start, measured to work.
inline constexpr Milliseconds STEP_TAP_SPACING_WORKED(666);
//  After a battle's black ends, the overworld fades in and the game ignores
//  the D-pad this long. Not measured: pokefirered's fade from black is 16
//  frames, then the field takes input; a tap sent at once was lost.
inline constexpr Milliseconds BATTLE_EXIT_LOCKOUT(500);
//  A spinner tile (Rocket Hideout, Viridian Gym) slides the player a tile
//  per 8 frames (pokefirered PlayerGoSpin(): MOVE_SPEED_FAST_1). Not
//  measured on the console.
inline constexpr Milliseconds SPIN_TILE_TIME(133);


//  The walker's presses.

//  Turns a player facing another way, without a step.
inline constexpr Milliseconds TURN_TAP(40);
//  How long to wait after a turn tap for the turn to show.
inline constexpr Milliseconds TURN_SETTLE(300);
//  How many taps turn_to() tries before it takes the player as turned.
inline constexpr size_t MAX_TURN_TAPS = 3;
//  Turns the player to face something, short enough not to step.
inline constexpr Milliseconds FACE_HOLD(40);
//  The turn animation, before an A press reaches what is ahead.
inline constexpr Milliseconds FACE_SETTLE(300);
//  The release between a leg's hold and what comes next.
inline constexpr Milliseconds STEP_RELEASE(100);
//  A one-step tap (step_once()).
inline constexpr Milliseconds STEP_TAP(66);
//  The release after a step tap: the next one starts STEP_TAP +
//  STEP_TAP_RELEASE after it, past the lockout.
inline constexpr Milliseconds STEP_TAP_RELEASE(700);

//  An open leg on the Bicycle is counted out in taps, each a tile: a 40ms
//  press, released to a BIKE_TAP_PERIOD period, slower than the chains
//  that worked, so none is dropped.
inline constexpr Milliseconds BIKE_TAP(40);
inline constexpr Milliseconds BIKE_TAP_PERIOD(120);
//  After a tap, how long until the screen shows whether it moved: the
//  tile's 100ms and video lag. The Command File Runner's tap check judged
//  its first tap too early, as unmoved. Not measured: a guess.
inline constexpr Milliseconds BIKE_TAP_SETTLE(250);
//  SELECT, pressed to get on the registered Bicycle, and the wait after
//  it for the player to be up on it.
inline constexpr Milliseconds BIKE_SELECT_PRESS(160);
inline constexpr Milliseconds BIKE_SELECT_SETTLE(1200);

//  How long the overworld must show nothing (no box, fade or battle) after
//  a battle before the next tap: twice the lockout.
inline constexpr Milliseconds AFTER_BATTLE_QUIET(1000);
//  The wait after a slide's last tile before the next press: the spin's
//  last frames and the facing it leaves. Not measured.
inline constexpr Milliseconds SPIN_STOP_SETTLE(300);

static_assert(TURN_TAP <= LONGEST_TAP && FACE_HOLD <= LONGEST_TAP && STEP_TAP <= LONGEST_TAP);
static_assert(STEP_TAP + STEP_TAP_RELEASE > STEP_TAP_SPACING_WORKED);
static_assert(STEP_TAP_SPACING_WORKED > STEP_TAP_LOCKOUT);
static_assert(AFTER_BATTLE_QUIET > BATTLE_EXIT_LOCKOUT);
static_assert(BIKE_TAP_PERIOD > BIKE_TAP_PERIOD_WORKED && BIKE_TAP_PERIOD_WORKED > BIKE_TAP_DROP_PERIOD);
static_assert(BIKE_TAP < BIKE_TILE_TIME && BIKE_TAP <= LONGEST_TAP);
static_assert(BIKE_SELECT_SETTLE > BIKE_SELECT_TIME);


} // namespace MovementMechanics
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
