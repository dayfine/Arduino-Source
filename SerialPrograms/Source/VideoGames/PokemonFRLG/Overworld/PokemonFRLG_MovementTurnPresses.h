/*  Movement Turn Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walker's presses that turn the player: how long, how long to wait, how many tries (PokemonFRLG_MovementMechanics.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MovementTurnPresses_H
#define PokemonAutomation_PokemonFRLG_MovementTurnPresses_H

#include <cstddef>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_MoveMechanics.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace MovementMechanics{


//  Turns a player facing another way, without a step.
inline constexpr Milliseconds TURN_TAP = Pokemon::FRLG_MOVE_MECHANICS.turn_tap;
//  How long to wait after a turn tap for the turn to show.
inline constexpr Milliseconds TURN_SETTLE(300);
//  How many taps turn_to() tries before it takes the player as turned.
inline constexpr size_t MAX_TURN_TAPS = 3;
//  Turns the player to face something, short enough not to step.
inline constexpr Milliseconds FACE_HOLD(40);
//  The turn animation, before an A press reaches what is ahead.
inline constexpr Milliseconds FACE_SETTLE(300);


} // namespace MovementMechanics
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
