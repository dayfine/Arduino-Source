/*  Overworld Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
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

//  The farthest sight of the trainers on `maps` who may see `tile`; 0 if
//  none does.
int sight_onto(const std::vector<MapData>& maps, const MapPosition& tile){
    int sight = 0;
    for (const MapData& map : maps){
        if (map.name != tile.map){
            continue;
        }
        for (const MapObject& object : map.objects){
            if (trainer_may_see(map, object, tile.point)){
                sight = std::max(sight, object.trainer->sight);
            }
        }
    }
    return sight;
}

//  Where a trainer stopped `step`, if the maps say: the one tile of the leg
//  a trainer may see. Not on the grass, where a stop may be a wild
//  battle's, nor on a leg to a warp, which is held on to its warp instead.
std::optional<MapPosition> trainer_stop(const std::optional<TrainerCrossing>& crossing, const RouteStep& step){
    if (!crossing || crossing->seen.size() != 1 || step.wild || step.warps){
        return std::nullopt;
    }
    return crossing->seen[0];
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
    void trainer_stopped(const RouteStep& step, const MapPosition& at) const{
        m_calls.log(ROUTE_LOG_TAG + Pokemon::route_replan_line(m_label, route_log_leg(step), "stopped by a trainer who saw the player", route_tiles(at)));
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

//  Walks each step through `calls`; watches a leg that crosses a trainer's
//  sight for the trainer; logs a leg to a wall seen interrupted after its
//  hold, which run_route() walks again, and checks a leg to a wall that
//  ended did end at its wall.
class CheckedSteps{
public:
    CheckedSteps(const RouteCalls& calls, const RouteLogLines& lines, WallCheck wall_check)
        : m_calls(calls)
        , m_lines(lines)
        , m_wall_check(wall_check)
    {}
    StepEnd walk(const RouteStep& step, std::optional<Direction> facing, const std::optional<TrainerCrossing>& crossing){
        const std::string leg = to_string(step);
        m_reholds = m_last_late && leg == m_last ? m_reholds + 1 : 0;
        StepEnd end = m_calls.walk(step, facing);
        if (end == StepEnd::DONE && crossing && m_calls.watch_for_trainer){
            end = m_calls.watch_for_trainer(step, crossing->approach_tiles);
        }
        m_last = leg;
        m_last_late = end == StepEnd::LATE;
        if (step.to_wall && end == StepEnd::LATE && !trainer_stop(crossing, step)){
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

//  run_route()'s walk: the plan, as rejoined or planned again after a
//  stop, which step is next, where the player is and which way they face.
class RouteRun{
public:
    RouteRun(const std::vector<RouteStep>& route, std::optional<Direction> facing, const RoutePlanning* planning, const RouteCalls& calls)
        : m_planning(planning)
        , m_calls(calls)
        , m_lines(calls, planning)
        , m_steps(calls, m_lines, planning != nullptr ? planning->wall_check : WallCheck::TAP)
        , m_plan(route)
        , m_at(planning != nullptr ? planning->start : std::nullopt)
        , m_facing(facing)
    {}

    void run(){
        m_lines.plan(m_at, m_plan);
        while (m_next < m_plan.size()){
            const RouteStep step = m_plan[m_next];
            m_lines.leg(m_next, m_plan);
            //  Where a hold that finds the player starts isn't known.
            const std::optional<TrainerCrossing> crossing = m_next >= m_holds ? crossing_here(step) : std::nullopt;
            const StepEnd end = m_steps.walk(step, m_facing, crossing);
            if (end == StepEnd::DONE){
                m_at = step.end;
                m_facing = step.facing_after;
                m_lines.done(step, ++m_next, m_holds);
                continue;
            }
            m_facing = std::nullopt;     //  not known after a stop
            if (++m_stops > MAX_ROUTE_STOPS){
                m_calls.fail(std::format("walk_overworld_route(): stopped part-way {} times.", m_stops));
                return;
            }
            if (!after_trainer(step, crossing) && !after_stop(step, end)){
                return;
            }
        }
    }

private:
    std::optional<TrainerCrossing> crossing_here(const RouteStep& step) const{
        if (!m_at || m_planning == nullptr || m_planning->maps == nullptr){
            return std::nullopt;
        }
        return trainer_crossing(*m_planning->maps, *m_at, step);
    }

    //  A stop the maps place (trainer_stop()): the trainer walks up to the
    //  player, who keeps facing the leg's way (Camper Liam, console B,
    //  2026-11-01), so the route is planned again from there. False if the
    //  maps don't place it or there is no way on.
    bool after_trainer(const RouteStep& step, const std::optional<TrainerCrossing>& crossing){
        const std::optional<MapPosition> stop = trainer_stop(crossing, step);
        if (!stop || m_planning == nullptr || m_planning->maps == nullptr){
            return false;
        }
        std::optional<std::vector<RouteStep>> way = plan_overworld_route(
            *m_planning->maps, *stop, RouteGoal{step.end, std::nullopt}, m_planning->costs, step.direction
        );
        if (!way){
            return false;
        }
        way->insert(way->end(), m_plan.begin() + m_next + 1, m_plan.end());
        m_lines.trainer_stopped(step, *stop);
        m_plan = std::move(*way);
        m_at = stop;
        m_facing = step.direction;
        m_holds = 0;
        m_next = 0;
        m_lines.plan(m_at, m_plan);
        return true;
    }

    //  Any other stop. False once the route has failed.
    bool after_stop(const RouteStep& step, StepEnd end){
        if (step.to_wall && end == StepEnd::LATE){
            return true;    //  walking it again ends at its wall
        }
        if (step.to_wall){
            //  Interrupted at once, hold after hold: by something on the
            //  way that speaks, a sign, which the plan didn't have there.
            m_calls.fail("walk_overworld_route(): a hold toward a wall is interrupted right away, again: the player is off the route, on " + to_string(step) + ".");
            return false;
        }
        if (end == StepEnd::STOPPED && step.warps && !step.wild){
            m_calls.hold_to_warp(step);
            m_at = step.end;
            m_facing = step.facing_after;
            m_next++;
            m_lines.end(step, m_at, EndBasis::ASSUMED, "stopped, then held on to the warp");
            return true;
        }
        m_lines.end(step, std::nullopt, EndBasis::UNKNOWN, "stopped part-way");
        Rejoined rejoined = rejoin(m_plan, m_next, m_at, m_planning, m_calls, m_lines);
        m_plan = std::move(rejoined.steps);
        m_holds = rejoined.holds;
        m_next = 0;
        return true;
    }

    const RoutePlanning* m_planning;
    const RouteCalls& m_calls;
    const RouteLogLines m_lines;
    CheckedSteps m_steps;
    std::vector<RouteStep> m_plan;
    std::optional<MapPosition> m_at;
    std::optional<Direction> m_facing;
    size_t m_next = 0;
    size_t m_stops = 0;
    size_t m_holds = 0;     //  of the plan, holds to walls that find the player
};

}


std::optional<TrainerCrossing> trainer_crossing(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step
){
    if (step.action != RouteAction::WALK){
        return std::nullopt;
    }
    TrainerCrossing crossing;
    const std::vector<MapPosition> tiles = leg_tiles(maps, start, step);
    for (size_t c = 1; c < tiles.size(); c++){
        const int sight = sight_onto(maps, tiles[c]);
        if (sight > 0){
            crossing.seen.push_back(tiles[c]);
            crossing.approach_tiles = std::max(crossing.approach_tiles, sight);
        }
    }
    if (crossing.seen.empty()){
        return std::nullopt;
    }
    return crossing;
}

void run_route(
    const std::vector<RouteStep>& route, std::optional<Direction> facing,
    const RoutePlanning* planning, const RouteCalls& calls
){
    RouteRun(route, facing, planning, calls).run();
}



}
}
}
