/*  Overworld Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::EndBasis;
using Pokemon::ROUTE_LOG_TAG;

std::vector<Pokemon::RouteTile> route_tiles(const std::optional<MapPosition>& position){
    if (!position){
        return {};
    }
    return {{position->map, position->point}};
}

Pokemon::RouteLogLeg route_log_leg(const RouteStep& step){
    Pokemon::RouteLogLeg leg{.action = Pokemon::RouteLegAction::WALK, .direction = step.direction, .tiles = (size_t)step.tiles, .end = Pokemon::RouteLegEnd::OPEN};
    if (step.action == RouteAction::SURF){
        leg.action = Pokemon::RouteLegAction::SURF;
    }else if (step.action == RouteAction::FACE){
        leg.action = Pokemon::RouteLegAction::FACE;
    }else if (step.warps){
        leg.end = Pokemon::RouteLegEnd::WARP;
    }else if (step.to_wall){
        leg.end = Pokemon::RouteLegEnd::WALL;
    }
    return leg;
}

//  The route's lines in the route log the games share, as BDSP's walker
//  writes them, under the caller's label.
class RouteLogLines{
public:
    RouteLogLines(const RouteCalls& calls, const RoutePlanning* planning)
        : m_calls(calls)
        , m_label(planning != nullptr ? planning->label : "Route")
    {}
    void plan(const std::optional<MapPosition>& from, const std::vector<RouteStep>& steps) const{
        std::vector<Pokemon::PlannedRouteLeg> legs;
        for (const RouteStep& step : steps){
            legs.push_back({.leg = route_log_leg(step), .after = route_tiles(step.end)});
        }
        m_calls.log(ROUTE_LOG_TAG + Pokemon::route_plan_line(m_label, route_tiles(from), legs));
    }
    void leg(size_t index, const std::vector<RouteStep>& steps) const{
        const RouteStep& step = steps[index];
        m_calls.log(ROUTE_LOG_TAG + Pokemon::route_leg_line(index + 1, steps.size(), {.leg = route_log_leg(step), .after = route_tiles(step.end)}));
    }
    void end(const RouteStep& step, const std::optional<MapPosition>& at, EndBasis basis, const std::string& why) const{
        m_calls.log(ROUTE_LOG_TAG + Pokemon::route_leg_end_line(m_label, route_log_leg(step), route_tiles(at), basis, why));
    }
    //  Step `number` (from 1) of a plan whose first `holds` steps are holds
    //  to walls, walked to its end.
    void done(const RouteStep& step, size_t number, size_t holds) const{
        if (number < holds){
            //  Where a hold but the last ends depends on where it started.
            end(step, std::nullopt, EndBasis::UNKNOWN, "a hold from an unknown tile");
            return;
        }
        end(step, step.end, EndBasis::WALKED, "");
        if (number == holds){
            m_calls.log(ROUTE_LOG_TAG + Pokemon::route_found_line(m_label, route_tiles(step.end), holds));
        }
    }

private:
    const RouteCalls& m_calls;
    std::string m_label;
};

//  Walks each step through `calls`; logs a leg to a wall seen interrupted
//  after its hold, which run_route() walks again, and checks a leg to a wall
//  that ended did end at its wall.
class CheckedSteps{
public:
    CheckedSteps(const RouteCalls& calls, const RouteLogLines& lines, WallCheck wall_check)
        : m_calls(calls)
        , m_lines(lines)
        , m_wall_check(wall_check)
    {}
    StepEnd walk(const RouteStep& step, std::optional<Direction> facing){
        const std::string leg = to_string(step);
        m_reholds = m_last_late && leg == m_last ? m_reholds + 1 : 0;
        const StepEnd end = m_calls.walk(step, facing);
        m_last = leg;
        m_last_late = end == StepEnd::LATE;
        if (step.to_wall && end == StepEnd::LATE){
            m_lines.end(step, std::nullopt, EndBasis::UNKNOWN, "something seen after the hold; walking it again");
        }
        if (end == StepEnd::DONE){
            check_wall(step);
        }
        return end;
    }

private:
    //  TODO(forest drift, 2026-11-01): diagnostic; remove or turn into a recovery once the cause is known.
    //  Four runs ended the walk to Viridian Forest's north gate off its
    //  column, which 52 legs walked by hand didn't reproduce: one more tap
    //  toward a leg's wall moves nobody who stands at it.
    void check_wall(const RouteStep& step) const{
        if (!step.to_wall || step.warps || step.action != RouteAction::WALK
            || m_wall_check == WallCheck::SKIP || !m_calls.moved_past_wall
        ){
            return;
        }
        if (!m_calls.moved_past_wall(step)){
            return;
        }
        m_calls.log(std::format(
            "Position check: {} moved on past where its wall should be; believed at {}, after {} re-hold(s) "
            "for something seen late (the leg's interrupts are the \"Leg:\" lines above).",
            to_string(step), to_string(step.end), m_reholds
        ));
        m_calls.fail("position check: " + to_string(step) + " did not end at its wall");
    }

    const RouteCalls& m_calls;
    const RouteLogLines& m_lines;
    WallCheck m_wall_check = WallCheck::TAP;
    std::string m_last;
    bool m_last_late = false;
    size_t m_reholds = 0;
};

//  A route rejoined after a stop: `holds` holds to walls, then the rest.
struct Rejoined{
    std::vector<RouteStep> steps;
    size_t holds = 0;
};

//  The route from `plan[stopped]`, stopped part-way from `start`: holds that
//  find the player, a way back to that step's end, then the rest as planned.
//  Logs it as a new plan, from a tile not known.
Rejoined rejoin(
    const std::vector<RouteStep>& plan, size_t stopped, const std::optional<MapPosition>& start,
    const RoutePlanning* planning, const RouteCalls& calls, const RouteLogLines& lines
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
    Rejoined ret{.steps = anchor->holds, .holds = anchor->holds.size()};
    ret.steps.insert(ret.steps.end(), back->begin(), back->end());
    ret.steps.insert(ret.steps.end(), plan.begin() + stopped + 1, plan.end());
    lines.plan(std::nullopt, ret.steps);
    return ret;
}

}


void run_route(
    const std::vector<RouteStep>& route, std::optional<Direction> facing,
    const RoutePlanning* planning, const RouteCalls& calls
){
    const RouteLogLines lines(calls, planning);
    CheckedSteps steps(calls, lines, planning != nullptr ? planning->wall_check : WallCheck::TAP);
    std::vector<RouteStep> plan = route;
    std::optional<MapPosition> at = planning != nullptr ? planning->start : std::nullopt;
    lines.plan(at, plan);
    size_t stops = 0;
    size_t holds = 0;   //  of the plan, holds to walls that find the player
    for (size_t i = 0; i < plan.size();){
        const RouteStep step = plan[i];
        lines.leg(i, plan);
        const StepEnd end = steps.walk(step, facing);
        if (end == StepEnd::DONE){
            at = step.end;
            facing = step.facing_after;
            lines.done(step, ++i, holds);
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
            lines.end(step, at, EndBasis::ASSUMED, "stopped, then held on to the warp");
            continue;
        }
        lines.end(step, std::nullopt, EndBasis::UNKNOWN, "stopped part-way");
        Rejoined rejoined = rejoin(plan, i, at, planning, calls, lines);
        plan = std::move(rejoined.steps);
        holds = rejoined.holds;
        i = 0;
    }
}



}
}
}
