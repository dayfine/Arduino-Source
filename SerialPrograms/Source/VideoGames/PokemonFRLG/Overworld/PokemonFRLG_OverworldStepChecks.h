/*  Overworld Step Checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a route step ended, and the checks that place the player after a
 *  leg that may have stopped short: a wall, an open leg, a warp.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldStepChecks_H
#define PokemonAutomation_PokemonFRLG_OverworldStepChecks_H

#include <algorithm>
#include <optional>
#include <string>
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldTrainerCrossing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How one step of a route ended.
enum class StepEnd{
    DONE,
    STOPPED,    //  stopped part-way: where the player is isn't known
    LATE,       //  something was seen after the hold, which may have
                //  stopped the player before the end
    //  Not walked: a wild battle set off before the step showed before it
    //  started. The step before it may have stopped part-way: its
    //  battle was seen too late (console B, 2026-11-03, Mt. Moon B2F).
    BATTLE_BEFORE,
};

//  Whether the screen moved while a step's hold ran, as the walker saw it.
enum class HoldMovement{
    UNMEASURED,     //  not measured: not a plain walk, or no screen to compare
    MOVED,          //  the map scrolled: at least one tile was walked
    NONE,           //  the map did not move at all
};

//  How a step ended, and for an open leg STOPPED part-way, how many of its
//  tiles the timing of the stop says the player walked. Not set: anywhere
//  on the leg. `movement` is for a DONE walk: a hold that moved nothing is
//  not trusted to have reached its end (run_route() judges it against the
//  tiles the plan expected).
struct StepOutcome{
    StepEnd end = StepEnd::DONE;
    std::optional<WalkedTiles> walked;
    //  STOPPED by a Bicycle tap that didn't move the player: the tile
    //  ahead of where they stand is blocked (count_bike_taps()).
    bool blocked = false;
    HoldMovement movement = HoldMovement::UNMEASURED;
};

//  Whether, after each leg to a wall, the walker taps once more toward the
//  wall and logs whether the player moved (see RouteCalls).
enum class WallCheck{
    TAP,
    SKIP,
};

//  The most tiles an open leg has been seen to end short with nothing on
//  screen: "west 6" in Viridian Forest ended 2 short (r16, console B,
//  2026-11-01); on Route 1 a column was lost (cp0006-b). The cause isn't
//  known (the forest drift), so this is what was seen, not a bound.
constexpr int OPEN_LEG_SHORTFALL = 2;

//  Where a leg onto a warp stopped when its fade was a wild battle's: the
//  battle's wipe passes for the fade (console B, 2026-11-03, Mt. Moon 1F),
//  so the stop isn't timed. The battle started on one of the leg's steps
//  before the warp. The walker reports the leg STOPPED there.
WalkedTiles walked_before_warp(const RouteStep& step);

//  Whether, after an open leg that ended with nothing seen, the player is
//  found again at walls when where they stand matters (see run_route()).
enum class OpenLegCheck{
    REANCHOR,
    TRUST,      //  taken to have ended where planned
};

//  The ways, east-west and north-south, the player may be a tile off the
//  plan along: from the route's start (an open leg of the route before, or
//  a scene, may leave them a tile off it), or after an open leg that way
//  that ended with nothing seen, until a leg to a wall that way, a warp or
//  a find places them (the fault sweeps put them a tile off after each open
//  leg). A hold to a wall along such a way may bump right away: 08.01's
//  "west 1 (to wall)" on Rock Tunnel 1F and 08.02's on Route 10.
class OpenLegDrift{
public:
    //  After `step` ended as planned.
    void walked(const RouteStep& step){
        if (step.warps){
            found();
        }else if (step.action == RouteAction::WALK){
            unsure(step.direction) = !step.to_wall;
        }
    }
    //  After the player was found on a tile.
    void found(){
        m_x = false;
        m_y = false;
    }
    //  The fewest tiles `step`, held from the tile the route has the player
    //  on, may walk: a tile fewer for a hold to a wall along a way unsure.
    int least_tiles(const RouteStep& step) const{
        const bool unsure_along = step.direction == Direction::EAST || step.direction == Direction::WEST ? m_x : m_y;
        return step.to_wall && unsure_along ? std::max(0, step.tiles - 1) : step.tiles;
    }

private:
    bool& unsure(Direction direction){
        return direction == Direction::EAST || direction == Direction::WEST ? m_x : m_y;
    }
    bool m_x = true;    //  east-west
    bool m_y = true;    //  north-south
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
