/*  Leg Planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plans a walk as straight legs: hold one direction, then the next. The
 *  game decides what a single press does through its step model; the
 *  planner searches, joins steps into legs, and prefers legs that end
 *  against a wall, because the player stops there however long the button
 *  is held, while a leg that ends in the open depends on timing. It can
 *  weigh more heavily a leg ending in the open that went through tiles
 *  where the walk is likely to be interrupted (tall grass): after an
 *  interruption a leg to a wall is simply walked again, while how far an
 *  open one got is a guess.
 *
 *  A step model is a class with:
 *
 *      using State = ...;      //  where the player is and what moves them:
 *                              //  comparable with ==
 *      using StateHash = ...;  //  a hash of State, e.g. std::hash<TilePos>
 *      std::optional<Step<State>> press(const State& state, Direction direction) const;
 *      bool held_at(const State& state, Direction direction) const;
 *
 *  press() is what pressing `direction` once does, or nothing if the player
 *  can't go that way (a wall, or something the plan must not do). held_at()
 *  is whether holding `direction` keeps the player on this tile: a wall, but
 *  not a person who may walk off or an edge to a map that isn't loaded.
 *
 *  A model may also have
 *
 *      int open_stop_risk(const State& state, Direction direction) const;
 *
 *  the extra cost of a leg that ends on `state` walking `direction` where
 *  the player could keep going: what a tile past the stop would cost, e.g.
 *  a tile a trainer may see, and
 *
 *      std::optional<int> tap_stop_risk(const State& state, Direction direction) const;
 *
 *  for such a leg of one step from where a hold to a wall ended, the extra
 *  cost to weigh instead of open_stop_risk() and LegCosts::per_open_wild_stop,
 *  or nothing to weigh those. A step can't stop part-way, so where an
 *  interruption stopping it would leave the player is no guess: e.g. on
 *  open water, a hold to a wall and a tile on starts a leg down a column
 *  with no wall of its own. Once a route: a run of them, wall to wall,
 *  would weave round the open stops it can't avoid.
 *
 */

#ifndef PokemonAutomation_Pokemon_LegPlanner_H
#define PokemonAutomation_Pokemon_LegPlanner_H

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What one press does.
template <typename State>
struct Step{
    State next;
    //  0 for a plain step. Anything else is a step of the game's own kind
    //  (e.g. starting to Surf), planned as a leg on its own.
    int action = 0;
    //  The step can't continue the leg before it, e.g. leaving the water,
    //  after which the player walks at another speed.
    bool starts_leg = false;
    //  No later step continues this leg: the game stops the player, e.g.
    //  at a warp.
    bool ends_leg = false;
    //  The step takes a warp: a door, a ladder, a cave entrance.
    bool warped = false;
    //  Which way the player faces afterwards. Usually the pressed direction;
    //  some warps turn the player.
    Direction facing = Direction::NORTH;
    //  Cost on top of LegCosts::per_tile: tall grass, a warp's fade, a dialog.
    int extra_cost = 0;
    //  The walk can be interrupted on this step, e.g. by a wild battle in
    //  tall grass. See LegCosts::per_open_wild_stop.
    bool wild = false;
};


// Category of navigation action in a route leg.
enum class LegKind{
    WALK,       //  hold `direction` for `tiles` steps
    ACTION,     //  one step of the game's own kind, `action`
    FACE,       //  turn to `direction` without moving
};

// Single continuous movement leg or turn action.
template <typename State>
struct Leg{
    LegKind kind = LegKind::WALK;
    int action = 0;
    Direction direction = Direction::NORTH;
    int tiles = 0;
    //  WALK: the last step takes a warp.
    bool warps = false;
    //  WALK: holding on keeps the player on the last tile, so the leg can be
    //  held for longer than it takes and walked again after an interruption.
    bool to_wall = false;
    //  A step of the leg is wild (Step::wild).
    bool wild = false;
    Direction facing_after = Direction::NORTH;
    State start;
    State end;
};


// Weights and penalties used during route planning.
struct LegCosts{
    int per_tile = 1;
    //  Extra for a step in a new direction.
    int per_turn = 2;
    //  Extra for ending a leg where the player could keep going: it has to
    //  be timed, and where an interruption leaves the player is a guess.
    int per_open_stop = 8;
    //  Extra again when that leg has a wild step, where an interruption is
    //  likely.
    int per_open_wild_stop = 0;
};


//  The cheapest legs from `start` to a state where `at_goal` holds, facing
//  `goal_facing` if given, or nothing if there is none. `start_facing` is
//  the way the player faces at the start, if known.
template <typename Model>
std::optional<std::vector<Leg<typename Model::State>>> plan_legs(
    const Model& model,
    const typename Model::State& start, std::optional<Direction> start_facing,
    const std::function<bool(const typename Model::State&)>& at_goal,
    std::optional<Direction> goal_facing,
    const LegCosts& costs = LegCosts()
);



namespace LegPlannerInternal{

//  A search node: the player, the way they face (4: not known), whether
//  they are in the middle of a leg that could go on, whether that leg had a
//  wild step, whether it is one step from where a hold to a wall ended, and
//  whether such a step was stopped after earlier (see tap_stop_risk() at
//  the top of this file).
template <typename State>
struct Node{
    State state;
    int facing = 4;
    bool moving = false;
    bool wild = false;
    bool tap_from_wall = false;
    bool tapped = false;

    bool operator==(const Node& other) const = default;
};

// Hash functor for planner search nodes.
template <typename State, typename StateHash>
struct NodeHash{
    size_t operator()(const Node<State>& node) const{
        return StateHash()(node.state) * 80 + node.facing * 16 + (node.moving ? 8 : 0) + (node.wild ? 4 : 0)
            + (node.tap_from_wall ? 2 : 0) + (node.tapped ? 1 : 0);
    }
};

// Edge transition between search nodes.
template <typename State>
struct Move{
    Node<State> from;
    Direction direction;
    Step<State> step;
};

// Cost incurred when stopping at a node.
template <typename Model, typename State>
int stop_cost(const Model& model, const Node<State>& node, const LegCosts& costs){
    if (!node.moving || model.held_at(node.state, (Direction)node.facing)){
        return 0;
    }
    if constexpr (requires { model.tap_stop_risk(node.state, Direction::NORTH); }){
        if (node.tap_from_wall && !node.tapped){
            if (const std::optional<int> risk = model.tap_stop_risk(node.state, (Direction)node.facing)){
                return costs.per_open_stop + *risk;
            }
        }
    }
    int cost = costs.per_open_stop + (node.wild ? costs.per_open_wild_stop : 0);
    if constexpr (requires { model.open_stop_risk(node.state, Direction::NORTH); }){
        cost += model.open_stop_risk(node.state, (Direction)node.facing);
    }
    return cost;
}

// Combines atomic movement steps into contiguous legs.
template <typename Model, typename State>
std::vector<Leg<State>> join_legs(const Model& model, const State& start, const std::vector<Move<State>>& moves){
    std::vector<Leg<State>> legs;
    bool can_extend = false;
    State before = start;
    for (const Move<State>& move : moves){
        const Step<State>& step = move.step;
        const bool extends = can_extend && step.action == 0 && !step.starts_leg
            && legs.back().kind == LegKind::WALK && legs.back().direction == move.direction;
        if (extends){
            legs.back().tiles++;
            legs.back().wild |= step.wild;
        }else{
            Leg<State> leg;
            leg.kind = step.action == 0 ? LegKind::WALK : LegKind::ACTION;
            leg.action = step.action;
            leg.direction = move.direction;
            leg.tiles = 1;
            leg.wild = step.wild;
            leg.start = before;
            legs.push_back(leg);
        }
        Leg<State>& leg = legs.back();
        leg.warps = step.warped;
        leg.to_wall = leg.kind == LegKind::WALK && !step.warped && model.held_at(step.next, move.direction);
        leg.facing_after = step.facing;
        leg.end = step.next;
        can_extend = step.action == 0 && !step.ends_leg;
        before = step.next;
    }
    return legs;
}

//  The search's working set. Nodes are kept in a list and queued by index,
//  so State needs no ordering.
template <typename Model>
struct Search{
    using State = typename Model::State;
    using Hash = NodeHash<State, typename Model::StateHash>;
    using Entry = std::pair<int, size_t>;

    std::unordered_map<Node<State>, int, Hash> distance;
    std::unordered_map<Node<State>, Move<State>, Hash> came_from;
    std::vector<Node<State>> nodes;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
};

// Looks up the distance to a search node.
template <typename Model, typename State>
int distance_of(const Search<Model>& search, const Node<State>& node){
    auto iter = search.distance.find(node);
    return iter == search.distance.end() ? std::numeric_limits<int>::max() : iter->second;
}

//  Records `cost` as the way to `node` if it is cheaper than any so far.
template <typename Model, typename State>
void relax(const Node<State>& node, int cost, const Move<State>& move, Search<Model>& search){
    if (cost >= distance_of(search, node)){
        return;
    }
    search.distance[node] = cost;
    search.came_from.insert_or_assign(node, move);
    search.nodes.push_back(node);
    search.queue.emplace(cost, search.nodes.size() - 1);
}

//  Queues every press from `current`, reached at `cost`.
template <typename Model, typename State>
void expand(
    const Model& model, const Node<State>& current, int cost,
    const LegCosts& costs, Search<Model>& search
){
    for (Direction direction : ALL_DIRECTIONS){
        std::optional<Step<State>> step = model.press(current.state, direction);
        if (!step){
            continue;
        }
        const bool continues = step->action == 0 && !step->ends_leg;
        const bool extends = current.moving && current.facing == (int)direction
            && step->action == 0 && !step->starts_leg;
        const bool from_wall = current.moving && model.held_at(current.state, (Direction)current.facing);
        const Node<State> next{
            step->next, (int)step->facing, continues, (extends && current.wild) || step->wild,
            !extends && from_wall, current.tapped || (!extends && current.tap_from_wall)
        };
        int step_cost = costs.per_tile + step->extra_cost;
        if (current.facing != (int)direction){
            step_cost += costs.per_turn + stop_cost(model, current, costs);
        }
        relax(next, cost + step_cost, Move<State>{current, direction, *step}, search);
    }
}

//  What it costs to end at `node`: stopping there, then turning to
//  `goal_facing`.
template <typename Model, typename State>
int arrival_cost(
    const Model& model, const Node<State>& node,
    std::optional<Direction> goal_facing, const LegCosts& costs
){
    int cost = stop_cost(model, node, costs);
    if (goal_facing && node.facing != (int)*goal_facing){
        cost += costs.per_turn;
    }
    return cost;
}

//  Runs the search from `first` and returns the goal node cheapest to end
//  at, or nothing if no goal can be reached.
template <typename Model, typename State>
std::optional<Node<State>> search_goal(
    const Model& model, const Node<State>& first,
    const std::function<bool(const State&)>& at_goal,
    std::optional<Direction> goal_facing, const LegCosts& costs, Search<Model>& search
){
    search.distance[first] = 0;
    search.nodes.push_back(first);
    search.queue.emplace(0, 0);

    std::optional<Node<State>> best_end;
    int best_total = std::numeric_limits<int>::max();
    while (!search.queue.empty()){
        const auto [cost, index] = search.queue.top();
        search.queue.pop();
        const Node<State> current = search.nodes[index];
        if (cost != distance_of(search, current)){
            continue;
        }
        if (cost >= best_total){
            break;
        }
        if (!at_goal(current.state)){
            expand(model, current, cost, costs, search);
            continue;
        }
        const int total = cost + arrival_cost(model, current, goal_facing, costs);
        if (total < best_total){
            best_total = total;
            best_end = current;
        }
    }
    return best_end;
}

//  The moves from `first` to `end`, in order.
template <typename Model, typename State>
std::vector<Move<State>> trace_moves(const Search<Model>& search, const Node<State>& first, const Node<State>& end){
    std::vector<Move<State>> moves;
    for (Node<State> node = end; !(node == first); node = search.came_from.at(node).from){
        moves.push_back(search.came_from.at(node));
    }
    std::reverse(moves.begin(), moves.end());
    return moves;
}

// Constructs a leg that faces a direction in place.
template <typename State>
Leg<State> face_leg(const State& state, Direction direction){
    Leg<State> face;
    face.kind = LegKind::FACE;
    face.direction = direction;
    face.facing_after = direction;
    face.start = state;
    face.end = state;
    return face;
}

} // namespace LegPlannerInternal



template <typename Model>
std::optional<std::vector<Leg<typename Model::State>>> plan_legs(
    const Model& model,
    const typename Model::State& start, std::optional<Direction> start_facing,
    const std::function<bool(const typename Model::State&)>& at_goal,
    std::optional<Direction> goal_facing,
    const LegCosts& costs
){
    using State = typename Model::State;
    using Node = LegPlannerInternal::Node<State>;

    const Node first{start, start_facing ? (int)*start_facing : 4, false, false, false, false};
    LegPlannerInternal::Search<Model> search;
    const std::optional<Node> end = LegPlannerInternal::search_goal(model, first, at_goal, goal_facing, costs, search);
    if (!end){
        return std::nullopt;
    }
    std::vector<Leg<State>> legs = LegPlannerInternal::join_legs(
        model, start, LegPlannerInternal::trace_moves(search, first, *end)
    );
    if (goal_facing && end->facing != (int)*goal_facing){
        legs.push_back(LegPlannerInternal::face_leg(end->state, *goal_facing));
    }
    return legs;
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
