/*  Movement Step Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walker's step taps and releases, and the quiet it waits for after a battle (PokemonFRLG_MovementMechanics.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MovementStepPresses_H
#define PokemonAutomation_PokemonFRLG_MovementStepPresses_H

#include <cstddef>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace MovementMechanics{


//  The release between a leg's hold and what comes next.
inline constexpr Milliseconds STEP_RELEASE(100);
//  A one-step tap (step_once()).
inline constexpr Milliseconds STEP_TAP(66);
//  The release after a step tap: the next one starts STEP_TAP +
//  STEP_TAP_RELEASE after it, past the lockout.
inline constexpr Milliseconds STEP_TAP_RELEASE(700);


//  How long the overworld must show nothing (no box, fade or battle) after
//  a battle before the next tap: twice the lockout.
inline constexpr Milliseconds AFTER_BATTLE_QUIET(1000);


} // namespace MovementMechanics
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
