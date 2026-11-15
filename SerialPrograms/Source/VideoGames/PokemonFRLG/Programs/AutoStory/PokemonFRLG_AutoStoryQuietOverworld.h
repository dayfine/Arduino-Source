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
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_InterruptHandlers.h"
#include "Pokemon/AutoStory/Pokemon_Settle.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether a look ended on FRLG's overworld at rest (QUIET_OVERWORLD: no
//  box, fade or battle) for at least `quiet`.
bool quiet_overworld(const Pokemon::ScreenSettle& settle, Milliseconds quiet);

//  FRLG's wait for the overworld at rest: the shared DOOR settle
//  (Pokemon::settle_rule()), its quiet the same second as
//  MovementMechanics::AFTER_BATTLE_QUIET, with a look every
//  BETWEEN_STEPS_LOOK and `cap`.
Pokemon::SettleRule quiet_overworld_rule(Milliseconds cap);

//  Waits with Pokemon::run_settle() and quiet_overworld_rule(`timeout`),
//  looking with `look`, fresh and then on from it. It only watches: no
//  button, and a battle still showing is waited out like any other
//  screen. After a battle, the next tap waits for this: one sent at its
//  black's end is lost (BATTLE_EXIT_LOCKOUT). `label` names the wait in a
//  timeout's text.
Pokemon::SettleEnd wait_for_quiet_overworld(
    const Pokemon::ScreenLookFunction& look, std::string_view label, Milliseconds timeout
);
//  How the wait ended, for the log: "quiet overworld", or why not.
std::string describe(const Pokemon::SettleEnd& end);

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
