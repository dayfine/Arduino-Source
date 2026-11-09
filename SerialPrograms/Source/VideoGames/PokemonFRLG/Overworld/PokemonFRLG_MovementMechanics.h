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
//  A one-step tap (step_once()) and the release after it: the next step
//  tap starts STEP_TAP + STEP_TAP_RELEASE after it, past the lockout.
inline constexpr Milliseconds STEP_TAP(66);
inline constexpr Milliseconds STEP_TAP_RELEASE(700);

static_assert(TURN_TAP <= LONGEST_TAP && FACE_HOLD <= LONGEST_TAP && STEP_TAP <= LONGEST_TAP);
static_assert(STEP_TAP + STEP_TAP_RELEASE > STEP_TAP_SPACING_WORKED);
static_assert(STEP_TAP_SPACING_WORKED > STEP_TAP_LOCKOUT);


} // namespace MovementMechanics
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
