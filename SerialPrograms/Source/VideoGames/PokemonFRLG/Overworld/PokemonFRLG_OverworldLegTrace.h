/*  Overworld Leg Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One [Leg] record (Pokemon_LegTrace.h) per RouteStep that run_route()
 *  walks, for devtools/maps/leg_trace.py and the replay of console failures.
 *  A leg starts where the route log says "Leg n/m" and ends where it says
 *  how it ended; the record says how that end is known:
 *
 *      FADE            a warp's fade was seen
 *      DEAD_RECKONED   taken to have ended as planned. FRLG's walker does not
 *                      measure whether the screen moved: a hold to a wall
 *                      that never moved the player ends at its planned tile
 *                      just the same (console B, 2026-11-12, 12.03
 *                      pallet_shore: two holds east from (6, 10) moved 0
 *                      tiles, and were logged "ended at (10, 10)" and "found
 *                      again at (12, 10)")
 *      SEEN            where the map puts a trainer who stopped the player
 *      UNKNOWN         the end is not known (stopped part-way, a hold from
 *                      an unknown tile)
 *
 *  The taps that moved (LegRecord::moved) are left out: the walker holds
 *  legs and counts nothing.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldLegTrace_H
#define PokemonAutomation_PokemonFRLG_OverworldLegTrace_H

#include <functional>
#include <optional>
#include <string>
#include "Pokemon/Navigation/Pokemon_LegTrace.h"
#include "PokemonFRLG_OverworldRouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The leg records of one run_route() (one event of one checkpoint), handed
//  to `log` as lines. A leg still open when the trace is destroyed is
//  written with its end unknown.
class OverworldLegTrace{
public:
    OverworldLegTrace(
        const std::string& checkpoint, const std::string& event,
        std::function<void(const std::string& line)> log
    );
    ~OverworldLegTrace();
    OverworldLegTrace(const OverworldLegTrace&) = delete;
    void operator=(const OverworldLegTrace&) = delete;

    //  `step` starts; the leg before it, if still open, ends unknown.
    void begin(const RouteStep& step);
    //  `step` ended as planned: on its warp's fade if it warps, else dead
    //  reckoned.
    void done(const RouteStep& step);
    //  The open leg ended on `at` as the map places a trainer who stopped
    //  the player there.
    void trainer_stopped(const MapPosition& at);
    //  The open leg's end is not known; `note` says why ("replan": the
    //  route is planned again from where the player may be).
    void unknown(const std::string& note);
    //  The open leg's hold went on to its warp, whose fade was seen.
    void held_to_warp(const RouteStep& step);
    //  A wild battle set off before the open leg started.
    void battle_before();

private:
    Pokemon::LegTracer m_tracer;
};


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
