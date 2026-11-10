/*  Route Scorer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <map>
#include <set>
#include "Pokemon_RouteScorer.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string to_string(const ScoredLeg& leg){
    const char* how = "taps";
    switch (leg.kind){
    case ScoredLegKind::TO_WALL:
        how = leg.run ? "run to wall" : "walk to wall";
        break;
    case ScoredLegKind::OPEN_HOLD:
        how = leg.run ? "run, open" : "walk, open";
        break;
    case ScoredLegKind::TAPS:
        break;
    }
    return std::format("{} {} ({})", direction_name(leg.direction), leg.tiles, how);
}

size_t tiles_walked_back(const std::vector<ScoredLeg>& legs){
    size_t tiles = 0;
    for (size_t i = 0; i < legs.size(); i++){
        const Direction back = Direction(((int)legs[i].direction + 2) % 4);
        for (size_t j = 0; j < i; j++){
            if (legs[j].direction == back){
                tiles += legs[i].tiles;
                break;
            }
        }
    }
    return tiles;
}

const char* verdict_name(RouteVerdict verdict){
    switch (verdict){
    case RouteVerdict::PASS:
        return "PASS";
    case RouteVerdict::FAIL:
        return "FAIL";
    case RouteVerdict::STRAND:
        return "STRAND";
    case RouteVerdict::RETRIES:
        return "RETRIES";
    }
    return "?";
}


namespace{

size_t distance(const TilePos& a, const TilePos& b){
    return (size_t)(std::abs(a.x - b.x) + std::abs(a.y - b.y));
}

bool stops_on(TileRisk risk){
    return risk == TileRisk::WARP || risk == TileRisk::TRIGGER;
}

//  One leg walked from one tile, nothing interrupting.
struct LegWalk{
    std::vector<TilePos> path;      //  tiles surely walked, in order, the first after the start
    std::vector<TilePos> ends;      //  where the leg may end
    long long ms = 0;
    bool stop = false;              //  ended at a wall (a re-localization)
    size_t taps = 0;
    std::string unsafe;             //  why the leg may end somewhere it must not
};

//  Steps from `from` until stopped, at most `most` steps; stops on a warp
//  or a trigger.
std::vector<TilePos> walk_steps(const ScoreMap& map, const TilePos& from, Direction direction, size_t most){
    std::vector<TilePos> path;
    TilePos at = from;
    while (path.size() < most){
        const std::optional<TilePos> next = map.step(at, direction);
        if (!next){
            break;
        }
        path.push_back(*next);
        at = *next;
        if (stops_on(map.risk(at))){
            break;
        }
    }
    return path;
}

size_t path_tiles(const TilePos& from, const std::vector<TilePos>& path){
    size_t tiles = 0;
    TilePos at = from;
    for (const TilePos& pos : path){
        tiles += distance(at, pos);
        at = pos;
    }
    return tiles;
}

LegWalk walk_to_wall(const ScoreMap& map, const TilePos& from, const ScoredLeg& leg, const RoutePaces& paces){
    LegWalk walk;
    walk.path = walk_steps(map, from, leg.direction, (size_t)-1);
    walk.ends = {walk.path.empty() ? from : walk.path.back()};
    walk.stop = walk.path.empty() || !stops_on(map.risk(walk.path.back()));
    walk.ms = paces.start_ms + (long long)path_tiles(from, walk.path) * paces.ms_per_tile(leg.direction, leg.run)
        + (walk.stop ? paces.stop_ms : 0);
    return walk;
}

LegWalk walk_taps(const ScoreMap& map, const TilePos& from, const ScoredLeg& leg, const RoutePaces& paces){
    LegWalk walk;
    walk.path = walk_steps(map, from, leg.direction, leg.tiles);
    walk.ends = {walk.path.empty() ? from : walk.path.back()};
    const bool blocked = walk.path.size() < leg.tiles && (walk.path.empty() || !stops_on(map.risk(walk.path.back())));
    //  A blocked walk of taps ends after two taps that move nothing.
    walk.taps = walk.path.size() + (blocked ? 2 : 0);
    walk.ms = (long long)walk.taps * paces.tap_ms;
    return walk;
}

//  Why a tile in a held leg's uncertain end must not be reached, or "".
std::string unsafe_overshoot(const ScoreMap& map, const TilePos& before, const TilePos& pos,
    const std::function<bool(const TilePos&)>& goal
){
    if (distance(before, pos) > 1){
        return "a ledge hop at " + to_string(pos);
    }
    switch (map.risk(pos)){
    case TileRisk::NONE:
        return "";
    case TileRisk::TRIGGER:
        return goal(pos) ? "" : "a trigger at " + to_string(pos);
    case TileRisk::WARP:
        return goal(pos) ? "" : "a warp at " + to_string(pos);
    case TileRisk::ENCOUNTER:
        return "grass at " + to_string(pos);
    case TileRisk::WANDERER:
        return "a wanderer's tile at " + to_string(pos);
    case TileRisk::SIGHT:
        return "a trainer's sight at " + to_string(pos);
    }
    return "";
}

LegWalk walk_open_hold(const ScoreMap& map, const TilePos& from, const ScoredLeg& leg,
    const RoutePaces& paces, const std::function<bool(const TilePos&)>& goal
){
    const size_t error = paces.open_hold_error ? paces.open_hold_error(leg.tiles) : 0;
    const size_t fewest = leg.tiles > error ? leg.tiles - error : 0;
    const std::vector<TilePos> all = walk_steps(map, from, leg.direction, leg.tiles + error);
    LegWalk walk;
    walk.path.assign(all.begin(), all.begin() + std::min(fewest, all.size()));
    for (size_t k = fewest; k <= all.size(); k++){
        walk.ends.push_back(k == 0 ? from : all[k - 1]);
        if (k > fewest && walk.unsafe.empty()){
            walk.unsafe = unsafe_overshoot(map, k == 1 ? from : all[k - 2], all[k - 1], goal);
        }
    }
    walk.ms = paces.start_ms + (long long)leg.tiles * paces.ms_per_tile(leg.direction, leg.run);
    return walk;
}

LegWalk walk_leg(const ScoreMap& map, const TilePos& from, const ScoredLeg& leg,
    const RoutePaces& paces, const std::function<bool(const TilePos&)>& goal
){
    LegWalk walk;
    switch (leg.kind){
    case ScoredLegKind::TO_WALL:
        walk = walk_to_wall(map, from, leg, paces);
        break;
    case ScoredLegKind::TAPS:
        walk = walk_taps(map, from, leg, paces);
        break;
    case ScoredLegKind::OPEN_HOLD:
        walk = walk_open_hold(map, from, leg, paces, goal);
        break;
    }
    walk.ms += paces.press_ms;
    return walk;
}


//  One way the walk may have gone so far, from one start tile.
struct Branch{
    TilePos pos;
    long long ms = 0;
    double weight = 0;      //  chance of this branch, the starts equally likely
    bool sighted = false;   //  a trainer has seen us (and been fought)
};

//  Where an interruption may happen, and what it costs.
struct Interruption{
    size_t leg;
    size_t steps;           //  steps of the leg walked when it happens
    TilePos from;           //  where the leg started
    TilePos at;
    double chance;
    long long cost_ms;
    double weight;
};

//  The plan from where an interruption leaves the player on: the rest of
//  the interrupted leg, then the legs after it.
std::vector<ScoredLeg> rest_of_plan(const std::vector<ScoredLeg>& legs, const Interruption& stop){
    const ScoredLeg& leg = legs[stop.leg];
    std::vector<ScoredLeg> rest;
    if (leg.kind == ScoredLegKind::TO_WALL || stop.steps < leg.tiles){
        ScoredLeg left = leg;
        left.tiles = leg.kind == ScoredLegKind::TO_WALL ? leg.tiles : leg.tiles - stop.steps;
        rest.push_back(left);
    }
    rest.insert(rest.end(), legs.begin() + stop.leg + 1, legs.end());
    return rest;
}

//  The tiles an interruption on a leg without a wall may leave us on: a
//  tap lost or one too many either way.
std::vector<TilePos> unsure_tiles(const ScoreMap& map, const ScoredLeg& leg, const Interruption& stop){
    std::vector<TilePos> tiles{stop.at};
    if (leg.kind == ScoredLegKind::TO_WALL){
        return tiles;
    }
    const std::vector<TilePos> back = walk_steps(map, stop.at, Direction((int(leg.direction) + 2) % 4), 1);
    const std::vector<TilePos> ahead = walk_steps(map, stop.at, leg.direction, 1);
    tiles.insert(tiles.end(), back.begin(), back.end());
    tiles.insert(tiles.end(), ahead.begin(), ahead.end());
    return tiles;
}

std::string key_of(std::vector<TilePos> tiles){
    std::sort(tiles.begin(), tiles.end(), [](const TilePos& a, const TilePos& b){
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    std::string key;
    for (const TilePos& pos : tiles){
        key += to_string(pos);
    }
    return key;
}


//  What a nominal walk of a plan gives: the branches at its end, the
//  interruptions it may meet, and how it may fail.
struct PlanWalk{
    std::vector<Branch> branches;
    std::vector<Interruption> interruptions;
    size_t stops = 0;
    size_t presses = 0;
    std::string fail;
};

class Scorer{
public:
    Scorer(const ScoreMap& map, const std::function<bool(const TilePos&)>& goal,
        const RoutePaces& paces, const InterruptModel& model, const RouteReplanner& replan
    )
        : m_map(map), m_goal(goal), m_paces(paces), m_model(model), m_replan(replan)
    {}

    RouteScore score(const std::vector<TilePos>& from, const std::vector<ScoredLeg>& legs, size_t depth);

private:
    PlanWalk walk_plan(const std::vector<TilePos>& from, const std::vector<ScoredLeg>& legs) const;
    void walk_branch(PlanWalk& out, const Branch& branch, size_t index, const ScoredLeg& leg,
        bool last, std::map<std::string, Branch>& next) const;
    void add_interruptions(PlanWalk& out, Branch& branch, size_t index, const TilePos& from, const LegWalk& walk) const;
    void recover(RouteScore& score, const std::vector<ScoredLeg>& legs, const Interruption& stop, size_t depth);
    RouteScore recovery_score(const std::vector<TilePos>& unsure, size_t depth);
    bool relocalizes(const std::vector<TilePos>& unsure, const std::vector<ScoredLeg>& legs) const;
    static void fail(RouteScore& score, RouteVerdict verdict, const std::string& why);

    const ScoreMap& m_map;
    const std::function<bool(const TilePos&)>& m_goal;
    const RoutePaces& m_paces;
    const InterruptModel& m_model;
    const RouteReplanner& m_replan;
    std::map<std::string, RouteScore> m_done;
    std::set<std::string> m_open;
    //  Recoveries in progress whose plan first stops every tile at one wall
    //  out of harm's way (relocalizes()).
    std::set<std::string> m_relocalizing;
};

//  Whether `legs` from `unsure` first walk every tile to one wall tile
//  where nothing interrupts. A recovery that does is a retry from a known
//  place: if it is interrupted where it was before, the walk does the same
//  again, each time with the interruption's chance, so it ends.
bool Scorer::relocalizes(const std::vector<TilePos>& unsure, const std::vector<ScoredLeg>& legs) const{
    if (legs.empty() || legs.front().kind != ScoredLegKind::TO_WALL){
        return false;
    }
    std::optional<TilePos> end;
    for (const TilePos& pos : unsure){
        const LegWalk walk = walk_to_wall(m_map, pos, legs.front(), m_paces);
        if (!walk.stop || (end && *end != walk.ends.front())){
            return false;
        }
        end = walk.ends.front();
    }
    return end && m_map.risk(*end) == TileRisk::NONE;
}

void Scorer::fail(RouteScore& score, RouteVerdict verdict, const std::string& why){
    if (score.verdict == RouteVerdict::PASS){
        score.verdict = verdict;
        score.why = why;
    }
}

void Scorer::add_interruptions(PlanWalk& out, Branch& branch, size_t index, const TilePos& from, const LegWalk& walk) const{
    for (size_t k = 0; k < walk.path.size(); k++){
        const TilePos& at = walk.path[k];
        Interruption stop{.leg = index, .steps = k + 1, .from = from, .at = at, .chance = 0, .cost_ms = 0, .weight = branch.weight};
        switch (m_map.risk(at)){
        case TileRisk::ENCOUNTER:
            stop.chance = m_model.encounter_per_tile;
            stop.cost_ms = m_model.encounter_ms;
            break;
        case TileRisk::WANDERER:
            stop.chance = m_model.wanderer_per_tile;
            stop.cost_ms = m_model.wanderer_ms;
            break;
        case TileRisk::SIGHT:
            stop.chance = branch.sighted ? 0 : 1;
            stop.cost_ms = m_model.trainer_ms;
            branch.sighted = true;
            break;
        default:
            break;
        }
        if (stop.chance > 0){
            out.interruptions.push_back(stop);
        }
    }
}

void Scorer::walk_branch(PlanWalk& out, const Branch& branch, size_t index, const ScoredLeg& leg,
    bool last, std::map<std::string, Branch>& next
) const{
    const LegWalk walk = walk_leg(m_map, branch.pos, leg, m_paces, m_goal);
    if (!walk.unsafe.empty() && out.fail.empty()){
        out.fail = std::format("leg {} ({}) from {} may overshoot onto {}", index + 1, to_string(leg), to_string(branch.pos), walk.unsafe);
    }
    for (const TilePos& pos : walk.path){
        if (m_map.risk(pos) == TileRisk::WARP && !(last && pos == walk.path.back() && m_goal(pos)) && out.fail.empty()){
            out.fail = std::format("leg {} ({}) from {} walks into the warp at {}", index + 1, to_string(leg), to_string(branch.pos), to_string(pos));
        }
    }
    Branch walked = branch;
    walked.ms += walk.ms;
    add_interruptions(out, walked, index, branch.pos, walk);
    for (const TilePos& end : walk.ends){
        Branch b = walked;
        b.pos = end;
        b.weight = walked.weight / (double)walk.ends.size();
        auto [it, added] = next.try_emplace(key_of({end}), b);
        if (!added){
            it->second.weight += b.weight;
            it->second.ms = std::max(it->second.ms, b.ms);
        }
    }
}

PlanWalk Scorer::walk_plan(const std::vector<TilePos>& from, const std::vector<ScoredLeg>& legs) const{
    PlanWalk out;
    for (const TilePos& pos : from){
        out.branches.push_back({.pos = pos, .ms = 0, .weight = 1.0 / (double)from.size(), .sighted = false});
    }
    for (size_t i = 0; i < legs.size(); i++){
        std::map<std::string, Branch> next;
        for (const Branch& branch : out.branches){
            walk_branch(out, branch, i, legs[i], i + 1 == legs.size(), next);
        }
        out.branches.clear();
        for (auto& item : next){
            out.branches.push_back(item.second);
        }
        if (out.branches.size() > m_model.max_spread && out.fail.empty()){
            out.fail = std::format("after leg {} the player may be on {} tiles", i + 1, out.branches.size());
        }
        out.presses += legs[i].kind == ScoredLegKind::TAPS ? legs[i].tiles : 1;
        out.stops += legs[i].kind == ScoredLegKind::TO_WALL ? 1 : 0;
    }
    for (const Branch& branch : out.branches){
        if (!m_goal(branch.pos) && out.fail.empty()){
            out.fail = "may end on " + to_string(branch.pos) + ", off the goal";
        }
    }
    return out;
}

RouteScore Scorer::recovery_score(const std::vector<TilePos>& unsure, size_t depth){
    RouteScore score;
    const std::string key = key_of(unsure);
    if (m_open.contains(key)){
        //  A retry from a wall: charged its replan alone (recover()).
        if (!m_relocalizing.contains(key)){
            fail(score, RouteVerdict::STRAND, "the recovery from " + key + " leads back to itself");
        }
        return score;
    }
    const auto done = m_done.find(key);
    if (done != m_done.end()){
        return done->second;
    }
    if (depth > m_model.max_recoveries){
        fail(score, RouteVerdict::RETRIES, std::format("more than {} recoveries in a row, at {}", m_model.max_recoveries, key));
        return score;
    }
    const std::optional<std::vector<ScoredLeg>> legs = m_replan ? m_replan(unsure) : std::nullopt;
    if (!legs){
        fail(score, RouteVerdict::FAIL, "no recovery from " + key);
        return score;
    }
    m_open.insert(key);
    if (relocalizes(unsure, *legs)){
        m_relocalizing.insert(key);
    }
    score = this->score(unsure, *legs, depth);
    m_open.erase(key);
    m_relocalizing.erase(key);
    if (score.verdict != RouteVerdict::PASS){
        score.why = "recovery from " + key + ": " + score.why;
    }
    m_done[key] = score;
    return score;
}

void Scorer::recover(RouteScore& score, const std::vector<ScoredLeg>& legs, const Interruption& stop, size_t depth){
    score.recoveries++;
    const ScoredLeg& leg = legs[stop.leg];
    const std::vector<ScoredLeg> rest = rest_of_plan(legs, stop);
    const PlanWalk resumed_exact = walk_plan({stop.at}, rest);
    const long long rest_ms = resumed_exact.branches.empty() ? 0 : resumed_exact.branches.front().ms;
    const std::vector<TilePos> unsure = unsure_tiles(m_map, leg, stop);
    const PlanWalk resumed = walk_plan(unsure, rest);
    long long extra_ms = m_paces.start_ms + m_paces.press_ms;
    long long extra_worst = extra_ms;
    if (!resumed.fail.empty()){
        score.replans++;
        const RouteScore again = recovery_score(unsure, depth + 1);
        if (again.verdict != RouteVerdict::PASS){
            fail(score, again.verdict, std::format("leg {} interrupted at {}: {}", stop.leg + 1, to_string(stop.at), again.why));
            return;
        }
        extra_ms = std::max<long long>(0, m_model.replan_ms + (long long)again.expected_ms - rest_ms);
        extra_worst = std::max<long long>(0, m_model.replan_ms + again.worst_ms - rest_ms);
    }
    score.expected_ms += stop.weight * stop.chance * (double)(stop.cost_ms + extra_ms);
    score.worst_ms = std::max(score.worst_ms, score.nominal_ms + stop.cost_ms + extra_worst);
}

RouteScore Scorer::score(const std::vector<TilePos>& from, const std::vector<ScoredLeg>& legs, size_t depth){
    RouteScore score;
    const PlanWalk walk = walk_plan(from, legs);
    score.stops = walk.stops;
    score.presses = walk.presses;
    for (const Branch& branch : walk.branches){
        score.nominal_ms = std::max(score.nominal_ms, branch.ms);
        score.expected_ms += branch.weight * (double)branch.ms;
    }
    score.worst_ms = score.nominal_ms;
    if (!walk.fail.empty()){
        fail(score, RouteVerdict::FAIL, walk.fail);
        return score;
    }
    for (const Interruption& stop : walk.interruptions){
        recover(score, legs, stop, depth);
    }
    return score;
}

} // namespace


RouteScore score_route(
    const ScoreMap& map, const std::vector<TilePos>& from,
    const std::function<bool(const TilePos&)>& goal,
    const std::vector<ScoredLeg>& legs,
    const RoutePaces& paces, const InterruptModel& model,
    const RouteReplanner& replan
){
    Scorer scorer(map, goal, paces, model, replan);
    return scorer.score(from, legs, 0);
}



} // namespace Pokemon
} // namespace PokemonAutomation
