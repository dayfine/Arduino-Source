/*  Overworld Bicycle Taps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  An open leg on the Bicycle counted out a tap at a time, each a tile,
 *  with the console side passed in (BikeTapCalls), so the walker and the
 *  sim share it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldBikeTaps_H
#define PokemonAutomation_PokemonFRLG_OverworldBikeTaps_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One tap of an open leg on the Bicycle, counted out a tile a tap: whether
//  the screen moved, and which of the leg's interrupts showed, if one did.
struct BikeTap{
    bool moved = false;
    std::optional<size_t> interrupt;
    //  The tap didn't move, and then an interrupt showed, which the tap
    //  cleared already: a trainer who saw the player, a battle seen late.
    bool cleared_late = false;
};
//  The console side of count_bike_taps(): `tap` taps once, for the leg's
//  `tap`th tile (from 1), and judges it;
//  `clear` clears interrupt i.
struct BikeTapCalls{
    std::function<BikeTap(int tap)> tap;
    std::function<void(size_t interrupt)> clear;
    std::function<void(const std::string& message)> log;
    //  Taps per tile before one that doesn't move counts as blocked: on
    //  foot a tap facing another way only turns the player
    //  (STEP_TAP_TRIES).
    int tries = 1;
};
//  Taps per tile on foot: the first tap of a leg may only turn, and the
//  game may miss a turn tap (MovementMechanics::MAX_TURN_TAPS).
constexpr int STEP_TAP_TRIES = 3;
//  How far a sure start may be off before a blocked tap finds the player:
//  three runs of 00.30 stood 3 tiles off (console B, 2026-11-09).
constexpr int BLOCKED_TAP_SLACK = 3;

//  Walks the open leg `step` a tap at a time: on the Bicycle each tap is
//  exactly a tile (MovementMechanics::BIKE_TAP). A tap that doesn't move
//  the player ends the leg there, blocked, and one with an interrupt ends
//  it once that is cleared. DONE once all of the leg's taps moved; else
//  STOPPED, with the tiles walked: blocked, the taps that moved, and up
//  to BLOCKED_TAP_SLACK more for a start that was off (run_route() keeps
//  the tiles with a blocked one ahead); interrupted, the taps that moved
//  and one more, which may have moved before its battle showed. A blind
//  leg (RouteStep::blind_taps) plays its taps, numbered from 0 with a turn
//  tap first, and is DONE.
StepOutcome count_bike_taps(const RouteStep& step, const BikeTapCalls& calls);

//  Whether `step` is walked on foot a tap at a time (RouteStep::tapped),
//  each a tile, by count_bike_taps() with STEP_TAP_TRIES: an open leg or
//  one to a wall or onto a spinner, not onto a warp or over a connection.
bool counts_step_taps(const RouteStep& step);

//  How long after a step tap of `step`, the `tap`th (from 1), the screen
//  is still: the step, and on the last tap of a leg onto a spinner the
//  slide (RouteStep::spin_tiles) and its settle.
Milliseconds step_tap_settle(const RouteStep& step, int tap);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
