/*  Robust Leg Planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans walks whose open legs may go a tile short or long. A leg to a wall
 *  is walked until the player stops; an open leg is walked for its tiles'
 *  worth of time and may end a few tiles either way. The planner searches
 *  over the sets of places the player may stand on after each leg, and
 *  later walls bring them back together, so the plan reaches the goal from
 *  every place the player may be.
 *
 *  A step model is a class with:
 *
 *      using State = ...;      //  where the player is: comparable with ==
 *      using StateLess = ...;  //  a strict order of State, for sets of them
 *      std::optional<State> step(const State& state, Direction direction) const;
 *      size_t step_tiles(const State& from, const State& to) const;
 *      bool held_at(const State& state, Direction direction) const;
 *      bool wild(const State& state) const;
 *      bool wander(const State& state) const;
 *
 *  step() is where one step `direction` lands, or nothing if the player is
 *  stopped. step_tiles() is how many tiles that step counts (a ledge hop
 *  counts 2). held_at() is whether the player, stopped, stays put however
 *  long the button is held (a wall); a stop that isn't held (a door, an exit
 *  off the maps) makes a leg the planner can't tell the end of, and it is
 *  not used. wild() is a place where a wild battle may stop the walk (tall
 *  grass); wander() one where someone wanders.
 *
 */

#ifndef PokemonAutomation_Pokemon_RobustLegPlanner_H
#define PokemonAutomation_Pokemon_RobustLegPlanner_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <utility>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A leg of a robust plan: walk `direction` for `tiles` tiles, or until
//  stopped if `to_wall`. `after` lists every place the player may stand on
//  once the leg is done.
template <typename State>
struct RobustLeg{
    Direction direction = Direction::NORTH;
    size_t tiles = 0;           //  tiles travelled; a ledge hop counts 2
    bool to_wall = false;
    std::vector<State> after;
};


//  The weights of a robust plan and the limits of its search.
struct RobustLegCosts{
    int per_tile = 1;
    int per_wild_tile = 4;      //  extra for each wild tile entered
    int per_wander_tile = 3;    //  extra for each tile entered where someone wanders
    int per_leg = 8;
    int leg_not_at_wall = 50;   //  extra for a leg that stops in the open
    //  An open leg of n tiles may end open_error + n / open_error_per tiles
    //  short or long.
    int open_error = 1;
    int open_error_per = 5;
    //  Open legs this many tiles or shorter land exactly. 0 keeps every
    //  open leg uncertain.
    int exact_open_tiles = 0;
    //  Extra per tile of an open leg: its timing error grows with its length.
    int per_open_tile = 0;
    int max_open_tiles = 12;    //  longest open leg to consider
    size_t max_spread = 5;      //  most places the player may be on after a leg
    //  Most sets of places to search before giving up. The search is over
    //  sets, not places, so it grows far faster than the map.
    size_t max_searched = 20000;
};


//  The cheapest legs that take the player from any of `from` to places that
//  all satisfy `goal`, even if every open leg ends up to its error off.
//  Legs that run along where someone wanders (two such places in a row, the
//  leg's start among them) are not used. Nothing if no such plan exists, or
//  none is found within `costs.max_searched` sets.
template <typename Model>
std::optional<std::vector<RobustLeg<typename Model::State>>> plan_robust_legs(
    const Model& model,
    std::vector<typename Model::State> from,
    const std::function<bool(const typename Model::State&)>& goal,
    const RobustLegCosts& costs
);

//  What plan_robust_legs() charges for walking `legs` from any of `from`,
//  each leg from the places the one before may end on. A leg it would not
//  plan costs its tiles and legs without wild or wander tiles.
template <typename Model>
long long robust_legs_cost(
    const Model& model,
    std::vector<typename Model::State> from,
    const std::vector<RobustLeg<typename Model::State>>& legs,
    const RobustLegCosts& costs
);



namespace RobustLegPlannerInternal{

//  Where the player stops walking `direction` until stopped.
template <typename State>
struct WallHit{
    State stop;
    size_t tiles = 0;
    bool held = false;
};
//  Walks `direction` from `from` until the player is stopped.
template <typename Model>
WallHit<typename Model::State> slide_to_wall(const Model& model, const typename Model::State& from, Direction direction){
    WallHit<typename Model::State> hit{from};
    while (true){
        const auto next = model.step(hit.stop, direction);
        if (!next){
            hit.held = model.held_at(hit.stop, direction);
            return hit;
        }
        hit.tiles += model.step_tiles(hit.stop, *next);
        hit.stop = *next;
    }
}

//  Where `from` ends up after `tiles` tiles `direction`, stopping early
//  where it is held, and the wild and wander tiles it enters. Nothing if a
//  step is stopped where the player is not held.
template <typename State>
struct Stepped{
    State end;
    int wild = 0;
    int wander = 0;
    bool along_wander = false;
};
//  Steps `from` up to `tiles` tiles `direction` (see Stepped).
template <typename Model>
std::optional<Stepped<typename Model::State>> step_tiles(const Model& model, typename Model::State from, Direction direction, int tiles){
    Stepped<typename Model::State> stepped{from};
    int travelled = 0;
    bool in_wander = model.wander(from);
    while (travelled < tiles){
        const auto next = model.step(from, direction);
        if (!next){
            if (!model.held_at(from, direction)){
                return std::nullopt;
            }
            break;
        }
        travelled += (int)model.step_tiles(from, *next);
        stepped.wild += model.wild(*next) ? 1 : 0;
        const bool next_wander = model.wander(*next);
        if (next_wander){
            stepped.wander++;
            stepped.along_wander |= in_wander;
        }
        in_wander = next_wander;
        from = *next;
    }
    stepped.end = from;
    return stepped;
}

//  Sorts a spread and drops repeated places, so equal sets compare equal.
template <typename Model>
void normalize(std::vector<typename Model::State>& spread){
    using State = typename Model::State;
    std::sort(spread.begin(), spread.end(), typename Model::StateLess());
    spread.erase(std::unique(spread.begin(), spread.end(), [](const State& a, const State& b){ return a == b; }), spread.end());
}
//  Orders spreads, for keeping them in a map.
template <typename Model>
struct SpreadLess{
    bool operator()(const std::vector<typename Model::State>& a, const std::vector<typename Model::State>& b) const{
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), typename Model::StateLess());
    }
};

//  A leg out of a spread: where every place of it ends, and its cost.
template <typename State>
struct Expansion{
    long long cost = 0;
    RobustLeg<State> leg;
};

//  To the wall: every place walks until stopped. Nothing unless each is
//  held, and one moves.
template <typename Model>
std::optional<Expansion<typename Model::State>> to_wall_leg(
    const Model& model, const std::vector<typename Model::State>& spread, Direction direction, const RobustLegCosts& costs
){
    Expansion<typename Model::State> expansion;
    size_t longest = 0;
    int wild = 0;
    int wander = 0;
    for (const auto& place : spread){
        const auto hit = slide_to_wall(model, place, direction);
        if (!hit.held){
            return std::nullopt;
        }
        if (const auto stepped = step_tiles(model, place, direction, (int)hit.tiles)){
            if (stepped->along_wander){
                return std::nullopt;
            }
            wild = std::max(wild, stepped->wild);
            wander = std::max(wander, stepped->wander);
        }
        longest = std::max(longest, hit.tiles);
        expansion.leg.after.push_back(hit.stop);
    }
    if (longest == 0){
        return std::nullopt;
    }
    expansion.cost = (long long)costs.per_leg + (long long)costs.per_tile * longest
        + (long long)costs.per_wild_tile * wild + (long long)costs.per_wander_tile * wander;
    expansion.leg.direction = direction;
    expansion.leg.tiles = longest;
    expansion.leg.to_wall = true;
    return expansion;
}

//  Open: about `tiles` tiles, give or take the error. Nothing if any
//  possible end is stopped where the player is not held.
template <typename Model>
std::optional<Expansion<typename Model::State>> open_leg(
    const Model& model, const std::vector<typename Model::State>& spread, Direction direction, int tiles, const RobustLegCosts& costs
){
    Expansion<typename Model::State> expansion;
    int wild = 0;
    int wander = 0;
    const int error = tiles <= costs.exact_open_tiles ? 0 : costs.open_error + tiles / costs.open_error_per;
    for (const auto& place : spread){
        for (int n = std::max(1, tiles - error); n <= tiles + error; n++){
            const auto stepped = step_tiles(model, place, direction, n);
            if (!stepped || stepped->along_wander){
                return std::nullopt;
            }
            wild = std::max(wild, stepped->wild);
            wander = std::max(wander, stepped->wander);
            expansion.leg.after.push_back(stepped->end);
        }
    }
    expansion.cost = (long long)costs.per_leg + costs.leg_not_at_wall
        + (long long)(costs.per_tile + costs.per_open_tile) * tiles
        + (long long)costs.per_wild_tile * wild + (long long)costs.per_wander_tile * wander;
    expansion.leg.direction = direction;
    expansion.leg.tiles = (size_t)tiles;
    return expansion;
}

//  Joins an open leg with the next one the same way: they are one hold.
template <typename State>
std::vector<RobustLeg<State>> consolidate(const std::vector<RobustLeg<State>>& legs){
    std::vector<RobustLeg<State>> consolidated;
    for (const RobustLeg<State>& leg : legs){
        if (!consolidated.empty() && consolidated.back().direction == leg.direction && !consolidated.back().to_wall){
            consolidated.back().tiles += leg.tiles;
            consolidated.back().to_wall = leg.to_wall;
            consolidated.back().after = leg.after;
        }else{
            consolidated.push_back(leg);
        }
    }
    return consolidated;
}

//  Dijkstra over sets of places the player may stand on.
template <typename Model>
class Search{
public:
    using State = typename Model::State;
    using Spread = std::vector<State>;

    Search(const Model& model, const RobustLegCosts& costs)
        : m_model(model)
        , m_costs(costs)
    {}

    std::optional<std::vector<RobustLeg<State>>> run(Spread start, const std::function<bool(const State&)>& goal){
        normalize<Model>(start);
        m_start = start;
        m_nodes[start] = Node{};
        m_queue.emplace(0, start);
        size_t searched = 0;
        while (!m_queue.empty()){
            const auto [cost, spread] = m_queue.top();
            m_queue.pop();
            Node& node = m_nodes[spread];
            if (node.done || node.cost != cost){
                continue;
            }
            node.done = true;
            if (++searched > m_costs.max_searched){
                return std::nullopt;
            }
            if (std::all_of(spread.begin(), spread.end(), goal)){
                return consolidate(legs_to(spread));
            }
            expand(spread, cost);
        }
        return std::nullopt;
    }

private:
    struct Node{
        long long cost = 0;
        Spread parent;
        RobustLeg<State> leg;
        bool done = false;
    };
    using Entry = std::pair<long long, Spread>;
    struct EntryGreater{
        bool operator()(const Entry& a, const Entry& b) const{
            return a.first > b.first;
        }
    };

    void expand(const Spread& spread, long long cost){
        for (Direction direction : ALL_DIRECTIONS){
            if (auto wall = to_wall_leg(m_model, spread, direction, m_costs)){
                relax(spread, cost, std::move(*wall));
            }
            for (int tiles = 1; tiles <= m_costs.max_open_tiles; tiles++){
                auto open = open_leg(m_model, spread, direction, tiles, m_costs);
                if (!open){
                    break;
                }
                relax(spread, cost, std::move(*open));
            }
        }
    }

    //  Only the start may be wider than costs.max_spread: every leg must
    //  bring the player within it.
    void relax(const Spread& spread, long long cost, Expansion<State> expansion){
        Spread& next = expansion.leg.after;
        normalize<Model>(next);
        if (next.size() > m_costs.max_spread || next == spread){
            return;
        }
        if (spread != m_start){
            const RobustLeg<State>& prev = m_nodes.at(spread).leg;
            if (expansion.leg.direction == prev.direction && !prev.to_wall){
                expansion.cost -= m_costs.per_leg;
            }
        }
        const long long total = cost + expansion.cost;
        auto iter = m_nodes.find(next);
        if (iter != m_nodes.end() && iter->second.cost <= total){
            return;
        }
        m_nodes[next] = Node{total, spread, expansion.leg};
        m_queue.emplace(total, std::move(next));
    }

    std::vector<RobustLeg<State>> legs_to(const Spread& end) const{
        std::vector<RobustLeg<State>> legs;
        for (Spread at = end; at != m_start; ){
            const Node& step = m_nodes.at(at);
            legs.push_back(step.leg);
            at = step.parent;
        }
        std::reverse(legs.begin(), legs.end());
        return legs;
    }

    const Model& m_model;
    const RobustLegCosts& m_costs;
    Spread m_start;
    std::map<Spread, Node, SpreadLess<Model>> m_nodes;
    std::priority_queue<Entry, std::vector<Entry>, EntryGreater> m_queue;
};

} // namespace RobustLegPlannerInternal



template <typename Model>
std::optional<std::vector<RobustLeg<typename Model::State>>> plan_robust_legs(
    const Model& model,
    std::vector<typename Model::State> from,
    const std::function<bool(const typename Model::State&)>& goal,
    const RobustLegCosts& costs
){
    return RobustLegPlannerInternal::Search<Model>(model, costs).run(std::move(from), goal);
}

template <typename Model>
long long robust_legs_cost(
    const Model& model,
    std::vector<typename Model::State> from,
    const std::vector<RobustLeg<typename Model::State>>& legs,
    const RobustLegCosts& costs
){
    using State = typename Model::State;
    long long total = 0;
    std::vector<State> spread = std::move(from);
    RobustLegPlannerInternal::normalize<Model>(spread);
    const RobustLeg<State>* previous = nullptr;
    for (const RobustLeg<State>& leg : legs){
        const auto expansion = leg.to_wall
            ? RobustLegPlannerInternal::to_wall_leg(model, spread, leg.direction, costs)
            : RobustLegPlannerInternal::open_leg(model, spread, leg.direction, (int)leg.tiles, costs);
        long long cost = (long long)costs.per_leg + (long long)costs.per_tile * (long long)leg.tiles;
        if (!leg.to_wall){
            cost += costs.leg_not_at_wall + (long long)costs.per_open_tile * (long long)leg.tiles;
        }
        if (expansion){
            cost = expansion->cost;
        }
        if (previous != nullptr && previous->direction == leg.direction && !previous->to_wall){
            cost -= costs.per_leg;
        }
        total += cost;
        spread = leg.after;
        RobustLegPlannerInternal::normalize<Model>(spread);
        previous = &leg;
    }
    return total;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
