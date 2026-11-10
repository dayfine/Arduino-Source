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
//  The console side of count_bike_taps(): `tap` taps once and judges it;
//  `clear` clears interrupt i.
struct BikeTapCalls{
    std::function<BikeTap()> tap;
    std::function<void(size_t interrupt)> clear;
    std::function<void(const std::string& message)> log;
};
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
//  and one more, which may have moved before its battle showed.
StepOutcome count_bike_taps(const RouteStep& step, const BikeTapCalls& calls);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
