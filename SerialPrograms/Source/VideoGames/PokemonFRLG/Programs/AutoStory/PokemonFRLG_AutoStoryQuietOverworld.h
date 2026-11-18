/*  AutoStory Quiet Overworld
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether, and waiting until, FRLG's overworld is at rest after a battle.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryQuietOverworld_H
#define PokemonAutomation_PokemonFRLG_AutoStoryQuietOverworld_H

#include <functional>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_InterruptHandlers.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"

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

//  Before a step whose first input is A on what the player faces (Surf,
//  Cut, Strength, the hideout's poster): a box still open would take that
//  A. GAME steps start on a message (step_interrupts()), so these clear it
//  themselves: on 12.03's surf_off_middle_sand_bar the look before the
//  step ended on "dialog box showing (quiet 0.3 s)", Surf's A closed the
//  box and no prompt came (console B, 2026-11-13). If `look` shows a box,
//  `clear_box` and wait for the quiet overworld (wait_for_quiet_overworld()).
//  Returns the line to log, or empty: another step, or no box.
std::string clear_box_before_a(
    const Pokemon::StoryStep& step,
    const Pokemon::ScreenLookFunction& look, const std::function<void()>& clear_box
);

//  `act`, after clear_box_before_a() with the look of `interrupts` and its
//  DIALOG handler (the walker's message clear), the line logged to
//  `logger`. `interrupts` and `logger` must outlive the action.
Pokemon::StoryActionFunction clearing_box_first(
    Pokemon::StoryActionFunction act, const Pokemon::StoryInterrupts& interrupts, Logger& logger
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
