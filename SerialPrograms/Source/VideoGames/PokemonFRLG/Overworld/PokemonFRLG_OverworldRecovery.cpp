/*  Overworld Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The route from `plan[stopped]`, stopped part-way from `start`: holds that
//  find the player, a way back to that step's end, then the rest as planned.
std::vector<RouteStep> rejoin(
    const std::vector<RouteStep>& plan, size_t stopped, const std::optional<MapPosition>& start,
    const RoutePlanning* planning, const RouteCalls& calls
){
    const RouteStep& step = plan[stopped];
    if (planning == nullptr || planning->maps == nullptr){
        calls.fail("walk_overworld_route(): stopped part-way on " + to_string(step) + ", with no maps to find the player again.");
        return {};
    }
    const std::optional<Reanchor> anchor = plan_reanchor(*planning->maps, start, step);
    if (!anchor){
        calls.fail("walk_overworld_route(): stopped part-way on " + to_string(step) + ", with no walls along it to find the player again.");
        return {};
    }
    const std::optional<std::vector<RouteStep>> back = plan_overworld_route(
        *planning->maps, anchor->end, RouteGoal{step.end, std::nullopt}, planning->costs, anchor->holds.back().direction
    );
    if (!back){
        calls.fail("walk_overworld_route(): no route from " + to_string(anchor->end) + " back to " + to_string(step.end) + ".");
        return {};
    }
    std::string holds;
    for (const RouteStep& hold : anchor->holds){
        holds += "; " + to_string(hold);
    }
    calls.log("Route: stopped part-way, finding the player again at " + to_string(anchor->end) + holds);
    std::vector<RouteStep> ret = anchor->holds;
    ret.insert(ret.end(), back->begin(), back->end());
    ret.insert(ret.end(), plan.begin() + stopped + 1, plan.end());
    return ret;
}

}


void run_route(
    const std::vector<RouteStep>& route, std::optional<Direction> facing,
    const RoutePlanning* planning, const RouteCalls& calls
){
    std::vector<RouteStep> plan = route;
    std::optional<MapPosition> at;
    size_t stops = 0;
    for (size_t i = 0; i < plan.size();){
        const RouteStep step = plan[i];
        const StepEnd end = calls.walk(step, facing);
        if (end == StepEnd::DONE){
            at = step.end;
            facing = step.facing_after;
            i++;
            continue;
        }
        facing = std::nullopt;     //  a trainer turns the player to face them
        if (++stops > MAX_ROUTE_STOPS){
            calls.fail(std::format("walk_overworld_route(): stopped part-way {} times.", stops));
            return;
        }
        if (step.to_wall && end == StepEnd::LATE){
            continue;   //  walking it again ends at its wall
        }
        if (step.to_wall){
            //  Interrupted at once, hold after hold: by something on the
            //  way that speaks, a sign, which the plan didn't have there.
            calls.fail("walk_overworld_route(): a hold toward a wall is interrupted right away, again: the player is off the route, on " + to_string(step) + ".");
            return;
        }
        if (end == StepEnd::STOPPED && step.warps && !step.wild){
            calls.hold_to_warp(step);
            at = step.end;
            facing = step.facing_after;
            i++;
            continue;
        }
        plan = rejoin(plan, i, at, planning, calls);
        i = 0;
    }
}



}
}
}
