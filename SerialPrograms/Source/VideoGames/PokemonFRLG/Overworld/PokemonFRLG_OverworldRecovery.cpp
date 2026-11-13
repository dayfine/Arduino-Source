/*  Overworld Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonFRLG_OverworldLegTrace.h"
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

//  The tiles an open `step` from `start` may have ended on with nothing
//  seen: its end, and up to OPEN_LEG_SHORTFALL tiles before it.
std::vector<StopCandidate> short_candidates(const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step){
    const std::vector<MapPosition> tiles = leg_tiles(maps, start, step);
    std::vector<StopCandidate> candidates;
    for (size_t c = 0; c <= (size_t)OPEN_LEG_SHORTFALL && c < tiles.size(); c++){
        candidates.push_back(StopCandidate{tiles[tiles.size() - 1 - c], std::nullopt});
    }
    return candidates;
}

//  Whether `next` is a hold to a wall.
bool is_wall_hold(const RouteStep& next){
    return next.action == RouteAction::WALK && next.to_wall && !next.warps;
}

//  Whether `next`, a hold to a wall, ends on its planned tile from each of
//  `candidates`: then it finds the player itself.
bool hold_finds(const std::vector<MapData>& maps, const std::vector<StopCandidate>& candidates, const RouteStep& next){
    for (const StopCandidate& candidate : candidates){
        const std::optional<WallHold> hold = hold_to_wall(maps, candidate.player, false, next.direction);
        if (!hold || !(hold->end == next.end)){
            return false;
        }
    }
    return true;
}

//  How many tiles `anchor`'s holds and way back walk, as far as planned.
int anchor_tiles(const Reanchor& anchor){
    int tiles = 0;
    for (const RouteStep& step : anchor.holds){
        tiles += step.tiles;
    }
    for (const RouteStep& step : anchor.back){
        tiles += step.tiles;
    }
    return tiles;
}

//  The route's lines in the route log the games share, as BDSP's walker
//  writes them, under the caller's label; and one [Leg] record per leg
//  (PokemonFRLG_OverworldLegTrace.h), written to the same log.
class RouteLogLines{
public:
    RouteLogLines(const RouteCalls& calls, const RoutePlanning* planning)
        : m_calls(calls)
        , m_label(planning != nullptr ? planning->label : "Route")
        , m_trace(
            planning != nullptr ? planning->checkpoint : "", m_label,
            [&calls](const std::string& line){ calls.log(line); }
        )
    {}
    void plan(const std::optional<MapPosition>& from, const std::vector<RouteStep>& steps) const{
        std::vector<Pokemon::PlannedRouteLeg> legs;
        for (const RouteStep& step : steps){
            legs.push_back({.leg = route_log_leg(step), .after = route_tiles(step.end)});
        }
        m_calls.log(std::string(ROUTE_LOG_TAG) + Pokemon::route_plan_line(m_label, route_tiles(from), legs));
    }
    void leg(size_t index, const std::vector<RouteStep>& steps){
        const RouteStep& step = steps[index];
        m_calls.log(std::string(ROUTE_LOG_TAG) + Pokemon::route_leg_line(index + 1, steps.size(), {.leg = route_log_leg(step), .after = route_tiles(step.end)}));
        m_trace.begin(step);
    }
    //  The leg ended other than as planned and walked: its hold went on to
    //  the warp (ASSUMED), or its end is not known (UNKNOWN).
    void end(const RouteStep& step, const std::optional<MapPosition>& at, EndBasis basis, const std::string& why){
        log_end(step, at, basis, why);
        if (basis == EndBasis::ASSUMED){
            m_trace.held_to_warp(step);
        }else{
            m_trace.unknown(why == "stopped part-way" ? "replan" : "again");
        }
    }
    void trainer_stopped(const RouteStep& step, const MapPosition& at){
        m_calls.log(std::string(ROUTE_LOG_TAG) + Pokemon::route_replan_line(m_label, route_log_leg(step), "stopped by a trainer who saw the player", route_tiles(at)));
        m_trace.trainer_stopped(at);
    }
    //  A wild battle set off before the leg that just started.
    void battle_before(){
        m_trace.battle_before();
    }
    void found(const MapPosition& at, size_t holds) const{
        m_calls.log(std::string(ROUTE_LOG_TAG) + Pokemon::route_found_line(m_label, route_tiles(at), holds));
    }
    //  Step `number` (from 1) of a plan whose first `holds` steps are holds
    //  to walls, walked to its end.
    void done(const RouteStep& step, size_t number, size_t holds){
        if (number < holds){
            //  Where a hold but the last ends depends on where it started.
            log_end(step, std::nullopt, EndBasis::UNKNOWN, "a hold from an unknown tile");
            m_trace.unknown("hold");
            return;
        }
        log_end(step, step.end, EndBasis::WALKED, "");
        m_trace.done(step);
        if (number == holds){
            found(step.end, holds);
        }
    }

private:
    void log_end(const RouteStep& step, const std::optional<MapPosition>& at, EndBasis basis, const std::string& why) const{
        m_calls.log(std::string(ROUTE_LOG_TAG) + Pokemon::route_leg_end_line(m_label, route_log_leg(step), route_tiles(at), basis, why));
    }

    const RouteCalls& m_calls;
    std::string m_label;
    OverworldLegTrace m_trace;
};

//  Walks each step through `calls`; watches a leg that crosses a trainer's
//  sight for the trainer, and logs whether a leg to a wall that ended did
//  end at its wall.
class CheckedSteps{
public:
    CheckedSteps(const RouteCalls& calls, WallCheck wall_check)
        : m_calls(calls)
        , m_wall_check(wall_check)
    {}
    StepOutcome walk(const RouteStep& step, std::optional<Direction> facing, const std::optional<TrainerCrossing>& crossing){
        const std::string leg = to_string(step);
        m_reholds = m_last_late && leg == m_last ? m_reholds + 1 : 0;
        StepOutcome outcome = m_calls.walk(step, facing);
        if (outcome.end == StepEnd::BATTLE_BEFORE){
            return outcome;
        }
        if (outcome.end == StepEnd::DONE && crossing && m_calls.watch_for_trainer){
            outcome = StepOutcome{.end = m_calls.watch_for_trainer(step, crossing->approach_tiles), .walked = std::nullopt};
        }
        m_last = leg;
        m_last_late = outcome.end == StepEnd::LATE;
        if (outcome.end == StepEnd::DONE){
            check_wall(step);
        }
        return outcome;
    }

private:
    //  TODO(forest drift, 2026-11-01): diagnostic; remove or turn into a recovery once the cause is known.
    //  Four runs ended the walk to Viridian Forest's north gate off its
    //  column, which 52 legs walked by hand didn't reproduce: one more tap
    //  toward a leg's wall moves nobody who stands at it. Only logged: the
    //  screen check can't tell a bump from a step yet, and failing on it
    //  stopped good runs.
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
            "Position check (logged only): a tap after {} moved the screen as a step would; believed at {}, after {} re-hold(s) "
            "for something seen late (the leg's interrupts are the \"Leg:\" lines above).",
            to_string(step), to_string(step.end), m_reholds
        ));
    }

    const RouteCalls& m_calls;
    WallCheck m_wall_check = WallCheck::TAP;
    std::string m_last;
    bool m_last_late = false;
    size_t m_reholds = 0;
};

//  run_route()'s walk: the plan, as rejoined or planned again after a
//  stop, which step is next, where the player is and which way they face.
class RouteRun{
public:
    RouteRun(const std::vector<RouteStep>& route, std::optional<Direction> facing, const RoutePlanning* planning, const RouteCalls& calls)
        : m_planning(planning)
        , m_calls(calls)
        , m_lines(calls, planning)
        , m_steps(calls, planning != nullptr ? planning->wall_check : WallCheck::TAP)
        , m_plan(with_warp_slack(route))
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
            const StepOutcome outcome = judge_movement(step, m_steps.walk(step, m_facing, crossing));
            if (outcome.end == StepEnd::DONE){
                const std::optional<MapPosition> start = m_next >= m_unchecked ? m_at : std::nullopt;
                remember_walked(step);
                m_at = step.end;
                m_facing = step.facing_after;
                m_lines.done(step, ++m_next, m_holds);
                check_open_leg(step, start);
                continue;
            }
            const std::optional<MapPosition> start = m_next >= m_holds ? m_at : std::nullopt;
            m_facing = std::nullopt;     //  not known after a stop
            if (++m_stops > MAX_ROUTE_STOPS){
                m_calls.fail(std::format("walk_overworld_route(): stopped part-way {} times.", m_stops));
                return;
            }
            if (outcome.end == StepEnd::BATTLE_BEFORE){
                if (!after_battle_before(step)){
                    return;
                }
                continue;
            }
            m_walked.reset();
            if (!after_stop(step, outcome, start, crossing)){
                return;
            }
        }
    }

private:
    const std::vector<MapData>* maps() const{
        return m_planning != nullptr ? m_planning->maps : nullptr;
    }

    std::optional<TrainerCrossing> crossing_here(const RouteStep& step) const{
        if (!m_at || maps() == nullptr){
            return std::nullopt;
        }
        return trainer_crossing(*maps(), *m_at, step);
    }

    //  A walk the next step's battle check may find stopped part-way:
    //  where in the plan it is, and where it started from, if known.
    struct Walked{
        RouteStep step;
        size_t index = 0;
        std::optional<MapPosition> start;
    };

    //  `step`, m_plan[m_next], walked to its end. Only a walk not onto a
    //  warp can have stopped part-way unseen.
    void remember_walked(const RouteStep& step){
        m_walked.reset();
        if (step.action == RouteAction::WALK && !step.warps){
            m_walked = Walked{step, m_next, m_next >= m_holds ? m_at : std::nullopt};
        }
    }

    //  A wild battle set off before `step`, m_plan[m_next], showed before
    //  it started. The step walked before it may have been stopped by the
    //  battle anywhere: the player is found again on it, as after a battle
    //  seen after its hold, and never taken to be on `step`'s line (the
    //  next leg was held from where the battle left the player, and the
    //  route went on 6 tiles off: console B, 2026-11-03, Mt. Moon B2F).
    //  With nothing walked since the plan was made, the player is where
    //  `step` starts. False once the route has failed.
    bool after_battle_before(const RouteStep& step){
        m_lines.battle_before();
        if (!m_walked){
            m_calls.log("Route: a wild battle before " + to_string(step) + ", with nothing walked since the plan: walking it from its start.");
            return true;
        }
        const Walked walked = std::move(*m_walked);
        m_walked.reset();
        m_calls.log("Route: a wild battle before " + to_string(step) + " was set off on " + to_string(walked.step) + ", which may have stopped part-way.");
        m_next = walked.index;
        m_at = walked.start;
        return after_stop(walked.step, StepOutcome{.end = StepEnd::LATE, .walked = std::nullopt}, walked.start, std::nullopt);
    }

    //  How many tiles the plan has `step`, a hold or an open leg, walk, at
    //  the least: its tiles from a tile the player is known to be on, and
    //  for the first hold that finds the player, the fewest from any tile
    //  they may be on. 0 where that isn't known, or if one of those tiles
    //  stands at the wall already (a bump right away moves nothing).
    int expected_tiles(const RouteStep& step) const{
        if (m_next >= m_holds){
            return step.tiles;
        }
        if (m_next != 0 || m_hold_from.empty() || maps() == nullptr){
            return 0;
        }
        int least = step.tiles;
        for (const MapPosition& from : m_hold_from){
            const std::optional<WallHold> hold = hold_to_wall(*maps(), from, false, step.direction);
            least = std::min(least, hold ? hold->tiles : 0);
        }
        return least;
    }

    //  A walk that ended DONE with the screen never having moved did not
    //  walk the tiles the plan expected: it is STOPPED at its start, never
    //  taken as walked or as at its wall (console B, 2026-11-12, 12.03
    //  pallet_shore: two holds east moved 0 tiles and were logged as
    //  ended). A hold that only bumped a wall it stood at moves nothing
    //  too, which expected_tiles() tells apart.
    StepOutcome judge_movement(const RouteStep& step, StepOutcome outcome){
        if (outcome.end != StepEnd::DONE || step.action != RouteAction::WALK || step.warps){
            return outcome;
        }
        if (outcome.movement != HoldMovement::NONE){
            m_unmoved.clear();
            return outcome;
        }
        const int expected = expected_tiles(step);
        if (expected < 1){
            m_calls.log("Route: " + to_string(step) + " moved the screen not at all, as a bump into the wall it may stand at already; taken as walked.");
            return outcome;
        }
        m_calls.log(std::format(
            "Route: {} moved the screen not at all, though the plan has it walk at least {} tiles; the player never moved.",
            to_string(step), expected
        ));
        return StepOutcome{.end = StepEnd::STOPPED, .walked = WalkedTiles{.min = 0, .max = 0}, .movement = HoldMovement::NONE};
    }

    //  After `step` moved nothing, from `start` if known: walked again
    //  from where it started, once. False once the route has failed.
    bool after_unmoved(const RouteStep& step, const std::optional<MapPosition>& start){
        const std::string leg = to_string(step);
        const bool again = m_unmoved == leg;
        m_unmoved = leg;
        m_lines.end(step, std::nullopt, EndBasis::UNKNOWN, "stopped part-way");
        if (again){
            m_calls.fail("walk_overworld_route(): " + leg + " moved nothing twice in a row: the player is not walking.");
            return false;
        }
        if (!start || maps() == nullptr){
            m_calls.fail("walk_overworld_route(): " + leg + " moved nothing, and the tile it was held from isn't known to walk it again from.");
            return false;
        }
        return rejoin(step, {StopCandidate{*start, std::nullopt}}, WalkedTiles{.min = 0, .max = 0});
    }

    //  After a stop on `step`, from `start` if known. False once the route
    //  has failed.
    bool after_stop(
        const RouteStep& step, const StepOutcome& outcome,
        const std::optional<MapPosition>& start, const std::optional<TrainerCrossing>& crossing
    ){
        if (outcome.end == StepEnd::STOPPED && outcome.movement == HoldMovement::NONE){
            return after_unmoved(step, start);
        }
        if (step.to_wall){
            return after_wall_stop(step, outcome, start, crossing);
        }
        if (outcome.end == StepEnd::STOPPED && step.warps && !step.wild){
            m_calls.hold_to_warp(step);
            m_at = step.end;
            m_facing = step.facing_after;
            m_next++;
            m_lines.end(step, m_at, EndBasis::ASSUMED, "stopped, then held on to the warp");
            return true;
        }
        m_lines.end(step, std::nullopt, EndBasis::UNKNOWN, "stopped part-way");
        if (maps() == nullptr){
            m_calls.fail("walk_overworld_route(): stopped part-way on " + to_string(step) + ", with no maps to find the player again.");
            return false;
        }
        const WalkedTiles walked = outcome.walked.value_or(whole_leg(step));
        return rejoin(step, stop_candidates(*maps(), start, step, walked), walked);
    }

    //  A leg to a wall seen interrupted after its hold is walked again: the
    //  wall stops the player at its end however far they got. Unless, off
    //  the grass, the maps place a trainer who saw them on one tile of it:
    //  they stopped there, and the route goes on from there (Camper Liam,
    //  console B, 2026-11-01).
    bool after_wall_stop(
        const RouteStep& step, const StepOutcome& outcome,
        const std::optional<MapPosition>& start, const std::optional<TrainerCrossing>& crossing
    ){
        if (outcome.end == StepEnd::STOPPED){
            //  Interrupted at once, hold after hold: by something on the
            //  way that speaks, a sign, which the plan didn't have there.
            m_calls.fail("walk_overworld_route(): a hold toward a wall is interrupted right away, again: the player is off the route, on " + to_string(step) + ".");
            return false;
        }
        if (crossing && !step.wild && maps() != nullptr){
            const std::vector<StopCandidate> candidates = stop_candidates(*maps(), start, step, whole_leg(step));
            if (candidates.size() == 1 && candidates[0].trainer){
                return rejoin(step, candidates, whole_leg(step));
            }
        }
        m_lines.end(step, std::nullopt, EndBasis::UNKNOWN, "something seen after the hold; walking it again");
        return true;
    }

    //  After `step`, an open leg from `start` (not known: not checked) that
    //  ended with nothing seen, while m_plan[m_next] is the step after it.
    //  A hold to a wall after a leg that ended short ends somewhere else
    //  with nothing to show it (r16), unless it ends on its tile from
    //  wherever the leg left the player. If it doesn't, finds the player at
    //  walls first; without such holds, or if they and the way back walk
    //  farther than the leg there and back, trusts the leg. Only there, and
    //  only that far, to keep the check cheap: in Viridian Forest (r16) it
    //  walks 10 tiles after "west 6", while after Route 1's "west 4" the
    //  only holds walk 30. An open leg after a short one walks on off the
    //  route, which isn't checked.
    void check_open_leg(const RouteStep& step, const std::optional<MapPosition>& start){
        if (maps() == nullptr || m_planning->open_leg_check != OpenLegCheck::REANCHOR || !start
            || step.action != RouteAction::WALK || step.to_wall || step.warps
            || m_next >= m_plan.size() || !is_wall_hold(m_plan[m_next])
        ){
            return;
        }
        const std::vector<StopCandidate> candidates = short_candidates(*maps(), *start, step);
        if (candidates.size() < 2 || hold_finds(*maps(), candidates, m_plan[m_next])){
            return;
        }
        const std::optional<Reanchor> anchor = plan_reanchor(*maps(), candidates, step, m_planning->costs);
        if (!anchor || anchor->holds.empty() || anchor_tiles(*anchor) > 2 * step.tiles){
            m_calls.log("Route: no holds to walls within " + std::to_string(2 * step.tiles) + " tiles find the player if " + to_string(step) + " ended short; taken as walked.");
            return;
        }
        m_calls.log(std::format(
            "Route: position check: {} may have ended up to {} tiles short with nothing seen; finding the player at walls.",
            to_string(step), OPEN_LEG_SHORTFALL
        ));
        replan(*anchor, m_next, candidates);
        m_at = std::nullopt;
        m_lines.plan(std::nullopt, m_plan);
    }

    //  The plan from here: `anchor`'s holds and way back, then the old
    //  plan's steps from `rest` on. Neither the holds nor the way back are
    //  checked as open legs again. The first hold starts from a tile of `from`.
    void replan(const Reanchor& anchor, size_t rest, const std::vector<StopCandidate>& from){
        m_hold_from.clear();
        for (const StopCandidate& candidate : from){
            m_hold_from.push_back(candidate.player);
        }
        std::vector<RouteStep> plan = anchor.holds;
        plan.insert(plan.end(), anchor.back.begin(), anchor.back.end());
        plan.insert(plan.end(), m_plan.begin() + rest, m_plan.end());
        m_plan = std::move(plan);
        m_walked.reset();
        m_holds = anchor.holds.size();
        m_unchecked = m_holds + anchor.back.size();
        m_next = 0;
    }

    //  Finds the player on one of `candidates`, plans the way back to the
    //  end of `step` (m_plan[m_next]), and the rest of the route after it.
    bool rejoin(const RouteStep& step, const std::vector<StopCandidate>& candidates, WalkedTiles walked){
        std::string where;
        for (const StopCandidate& candidate : candidates){
            where += (where.empty() ? "" : "; ") + to_string(candidate);
        }
        m_calls.log(std::format(
            "Route: {} stopped {} to {} tiles in; the player is on one of: {}.",
            to_string(step), walked.min, walked.max, where.empty() ? "nothing (no start to work from)" : where
        ));
        const std::optional<Reanchor> anchor = candidates.empty()
            ? std::nullopt
            : plan_reanchor(*maps(), candidates, step, m_planning->costs);
        if (!anchor){
            m_calls.fail(
                "walk_overworld_route(): stopped part-way on " + to_string(step)
                + ", and no holds to walls find the player from every tile they may be on."
            );
            return false;
        }
        replan(*anchor, m_next + 1, candidates);
        if (m_holds > 0){
            m_at = std::nullopt;
            m_lines.plan(std::nullopt, m_plan);
            return true;
        }
        //  Only one tile: the player stands there, still facing the leg's
        //  way (also after a trainer walked up: Camper Liam, console B,
        //  2026-11-01).
        if (candidates[0].trainer){
            m_lines.trainer_stopped(step, anchor->found);
        }else{
            m_lines.found(anchor->found, 0);
        }
        m_at = anchor->found;
        m_facing = step.direction;
        m_lines.plan(m_at, m_plan);
        return true;
    }

    const RoutePlanning* m_planning;
    const RouteCalls& m_calls;
    RouteLogLines m_lines;
    CheckedSteps m_steps;
    std::vector<RouteStep> m_plan;
    std::optional<MapPosition> m_at;
    std::optional<Direction> m_facing;
    size_t m_next = 0;
    size_t m_stops = 0;
    size_t m_holds = 0;     //  of the plan, holds to walls that find the player
    size_t m_unchecked = 0; //  of the plan, the holds and the way back after them
    std::optional<Walked> m_walked;     //  the step walked last, if it may have stopped part-way
    std::vector<MapPosition> m_hold_from;   //  the tiles the first hold of the plan may start from
    std::string m_unmoved;      //  the leg that moved nothing last, until another walks
};

} // namespace


WalkedTiles walked_before_warp(const RouteStep& step){
    return WalkedTiles{.min = 1, .max = step.tiles - 1};
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
