/*  AutoStory Quiet Overworld
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether, and waiting until, FRLG's overworld is at rest after a battle.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryQuietOverworld_H
#define PokemonAutomation_PokemonFRLG_AutoStoryQuietOverworld_H

#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_InterruptHandlers.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether a look ended on FRLG's overworld at rest: nothing showing (no
//  box, fade or battle; FrlgScreenObserver reports no PLAYER_CAN_MOVE, so
//  Pokemon::is_settled() never holds) for at least `quiet`.
bool quiet_overworld(const Pokemon::ScreenSettle& settle, Milliseconds quiet);

//  Looks with `look`, fresh and then on from it, until it ends on
//  quiet_overworld() for `quiet`, or has looked for `timeout`. Returns how
//  the last look ended. After a battle, the next tap waits for this
//  (MovementMechanics::AFTER_BATTLE_QUIET): one sent at its black's end
//  is lost (BATTLE_EXIT_LOCKOUT).
Pokemon::ScreenSettle wait_for_quiet_overworld(
    const Pokemon::ScreenLookFunction& look, Milliseconds quiet, Milliseconds timeout
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
