/*  Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What stopped the player, judged from the order of screen events
 *  (ScreenEventLog), the same way for every game: a wild encounter, a
 *  trainer's sight, a dialog, a warp. One frame cannot tell them apart; the
 *  order can. A white flash then a battle is an encounter, a white fade with
 *  no battle after it a cave exit (BDSP's walk fled one as a battle, #268);
 *  a box then a flash then a battle is a trainer's lines; a flash then a box
 *  then a battle is a wild battle's opening text.
 *
 *  Games differ in timing only, so they pass their numbers (InterruptKnobs)
 *  rather than their own classifier.
 *
 */

#ifndef PokemonAutomation_Pokemon_Interrupts_H
#define PokemonAutomation_Pokemon_Interrupts_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon_ScreenEvents.h"
#include "Pokemon_Transitions.h"
#include "Pokemon_WarpJudge.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What stopped the player.
enum class InterruptKind{
    NONE,
    WILD_ENCOUNTER,     //  A battle with no trainer's lines before it.
    TRAINER_SIGHT,      //  A dialog box, then a battle.
    DIALOG,             //  A dialog box and no battle.
    WARP,               //  A fade back up with no battle after it, or a banner.
    UNSETTLED,          //  A fade, too recent to know if a battle follows.
};
//  "trainer sight", for the log.
std::string interrupt_kind_name(InterruptKind kind);

//  What stopped the player, and the evidence for it.
struct InterruptVerdict{
    InterruptKind kind = InterruptKind::NONE;
    //  The changes it was judged on, in order.
    std::vector<ScreenEventChange> evidence = {};
    //  The log dropped changes the window reaches back to: the verdict may
    //  miss what came first.
    bool log_truncated = false;
};
//  "wild encounter: +0 ms white fade rose, +3400 ms battle started rose",
//  times from `origin`.
std::string describe(const InterruptVerdict& verdict, WallClock origin);

//  What interrupted the player in `window` of `log`. Order of precedence: a
//  battle, then a warp, then a dialog. Only events that rose in the window
//  count: a box already up when it starts is not this interrupt's.
InterruptVerdict classify_interrupt(
    const ScreenEventLog& log, InterruptWindow window, const InterruptKnobs& knobs
);


//  How long after the step that set an interrupt off one of its events
//  shows, as a game measured it on its console.
struct EventLag{
    ScreenEvent event = ScreenEvent::BATTLE_STARTED;
    Milliseconds lag = Milliseconds(0);
};

//  When the step that set `verdict`'s interrupt off was taken: the first
//  rise in its evidence that `lags` times, less that lag. The first, as it
//  shows nearest the step. Nothing if `lags` times none of them, as for a
//  game that hasn't measured its events, or for no verdict. Lags are one
//  kind of interrupt's (a wild battle's box, not a trainer's): the caller
//  checks the verdict's kind first.
//
//  This places an interrupt seen only after a walk's hold has ended, by
//  what the log saw of it during the hold, instead of by when the watcher
//  that stopped for it fired.
std::optional<WallClock> interrupt_step_time(const InterruptVerdict& verdict, const std::vector<EventLag>& lags);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
