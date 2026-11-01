/*  BDSP AutoStory - Route planner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <map>
#include <queue>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/Navigation/Pokemon_LegPlanner.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::TileKind;
using Pokemon::TilePos;


namespace{

size_t distance(const TilePos& a, const TilePos& b){
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

bool tall_grass(const StoryWorld& world, const TilePos& pos){
    return world.tile(pos) == TileKind::TALL_GRASS;
}

}


WallHit walk_to_wall(const StoryWorld& world, const TilePos& from, Direction direction){
    WallHit hit;
    hit.stop = from;
    while (true){
        const TilePos before = hit.stop;
        const std::optional<TilePos> next = world.step(before, direction);
        if (!next){
            hit.wall = before.next(direction);
            hit.anchored = world.at_wall(before, direction);
            return hit;
        }
        hit.tiles += distance(*next, before);
        hit.stop = *next;
    }
}


std::string to_string(const TileLeg& leg){
    return std::format("{} {}{}", Pokemon::direction_name(leg.direction), leg.tiles, leg.ends_at_wall ? " (to wall)" : " (open)");
}


namespace{

//  The world's step rules for the shared leg planner. A ledge hop is one
//  press that moves two tiles.
class StoryWorldModel{
public:
    using State = TilePos;
    using StateHash = std::hash<TilePos>;

    StoryWorldModel(const StoryWorld& world, const TilePlanCosts& costs)
        : m_world(world)
        , m_costs(costs)
    {}

    std::optional<Pokemon::Step<State>> press(const State& pos, Direction direction) const{
        const std::optional<TilePos> next = m_world.step(pos, direction);
        if (!next){
            return std::nullopt;
        }
        Pokemon::Step<State> step;
        step.next = *next;
        step.facing = direction;
        step.extra_cost = m_costs.per_tile * ((int)distance(*next, pos) - 1);
        if (tall_grass(m_world, *next)){
            step.extra_cost += m_costs.per_grass_tile;
        }
        return step;
    }
    bool held_at(const State& pos, Direction direction) const{
        return m_world.at_wall(pos, direction);
    }

private:
    const StoryWorld& m_world;
    const TilePlanCosts& m_costs;
};

void check_standable(const StoryWorld& world, const TilePos& from, const TilePos& to){
    if (!world.standable(from) || !world.standable(to)){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
            "Route ends must be standable: {} -> {}.", Pokemon::to_string(from), Pokemon::to_string(to)
        ));
    }
}

}

std::optional<std::vector<TileLeg>> plan_tile_route(
    const StoryWorld& world,
    const TilePos& from, const TilePos& to,
    const TilePlanCosts& costs
){
    check_standable(world, from, to);
    if (from == to){
        return std::vector<TileLeg>();
    }
    Pokemon::LegCosts leg_costs;
    leg_costs.per_tile = costs.per_tile;
    leg_costs.per_turn = costs.per_leg;
    leg_costs.per_open_stop = costs.leg_not_at_wall;
    const auto legs = Pokemon::plan_legs(
        StoryWorldModel(world, costs), from, std::nullopt,
        std::function<bool(const TilePos&)>([&](const TilePos& pos){ return pos == to; }),
        std::nullopt, leg_costs
    );
    if (!legs){
        return std::nullopt;
    }
    std::vector<TileLeg> ret;
    for (const Pokemon::Leg<TilePos>& leg : *legs){
        ret.push_back(TileLeg{leg.direction, distance(leg.end, leg.start), leg.to_wall});
    }
    return ret;
}

std::vector<TilePos> sight_tiles(const StoryWorld& world, const TrainerSight& trainer){
    std::vector<TilePos> tiles;
    TilePos pos = trainer.pos;
    for (int i = 0; i < trainer.range; i++){
        pos = pos.next(trainer.facing);
        if (!world.standable(pos)){
            break;
        }
        tiles.push_back(pos);
    }
    return tiles;
}

std::vector<std::vector<std::string>> legs_crossing_sight(
    const StoryWorld& world,
    const TilePos& from,
    const std::vector<TileLeg>& legs,
    const std::vector<TrainerSight>& trainers
){
    std::vector<std::vector<TilePos>> sights;
    for (const TrainerSight& trainer : trainers){
        sights.push_back(sight_tiles(world, trainer));
    }
    std::vector<std::vector<std::string>> crossings;
    TilePos pos = from;
    for (const TileLeg& leg : legs){
        const std::vector<TilePos> tiles = trace_legs(world, pos, {leg});
        std::vector<std::string> seen_by;
        for (size_t t = 0; t < trainers.size(); t++){
            const bool seen = std::any_of(tiles.begin() + 1, tiles.end(), [&](const TilePos& tile){
                return std::ranges::find(sights[t], tile) != sights[t].end();
            });
            if (seen){
                seen_by.push_back(trainers[t].name);
            }
        }
        crossings.push_back(std::move(seen_by));
        pos = tiles.back();
    }
    return crossings;
}

std::vector<TilePos> trace_legs(
    const StoryWorld& world,
    const TilePos& from,
    const std::vector<TileLeg>& legs
){
    std::vector<TilePos> tiles{from};
    for (const TileLeg& leg : legs){
        size_t travelled = 0;
        while (travelled < leg.tiles){
            const TilePos pos = tiles.back();
            const std::optional<TilePos> next = world.step(pos, leg.direction);
            if (!next){
                throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
                    "Leg {} is stopped at {}.", to_string(leg), Pokemon::to_string(pos)
                ));
            }
            travelled += distance(*next, pos);
            tiles.push_back(*next);
        }
        if (travelled != leg.tiles){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("Leg {} overshoots on a ledge hop.", to_string(leg)));
        }
    }
    return tiles;
}


std::string to_string(const PlannedLeg& leg){
    std::string out = to_string(leg.leg) + " to";
    for (const TilePos& pos : leg.after){
        out += " " + Pokemon::to_string(pos);
    }
    return out;
}

namespace{

//  Where `from` ends up after `tiles` steps in `direction`, stopping early at
//  anything that stops the player, and how many tall-grass tiles it enters.
//  Nothing if a step would leave the maps or enter a door: the plan cannot
//  tell what happens then.
struct Stepped{
    TilePos end;
    int grass = 0;
};
std::optional<Stepped> step_tiles(const StoryWorld& world, TilePos from, Direction direction, int tiles){
    Stepped stepped;
    int travelled = 0;
    while (travelled < tiles){
        const std::optional<TilePos> next = world.step(from, direction);
        if (!next){
            if (!world.at_wall(from, direction)){
                return std::nullopt;
            }
            break;
        }
        travelled += (int)distance(*next, from);
        if (tall_grass(world, *next)){
            stepped.grass++;
        }
        from = *next;
    }
    stepped.end = from;
    return stepped;
}

using Spread = std::vector<TilePos>;

bool spread_less(const TilePos& a, const TilePos& b){
    return a.y != b.y ? a.y < b.y : a.x < b.x;
}
void normalize(Spread& spread){
    std::sort(spread.begin(), spread.end(), spread_less);
    spread.erase(std::unique(spread.begin(), spread.end()), spread.end());
}
struct SpreadLess{
    bool operator()(const Spread& a, const Spread& b) const{
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), spread_less);
    }
};

//  A leg out of a spread: where every tile of it ends, and its cost.
struct Expansion{
    long long cost = 0;
    Spread next;
    TileLeg leg;
};

//  To the wall: every position walks until stopped. Nothing unless each is
//  held at a wall, and one moves.
std::optional<Expansion> to_wall_leg(const StoryWorld& world, const Spread& spread, Direction direction, const RobustPlanCosts& costs){
    Expansion expansion;
    size_t longest = 0;
    int grass = 0;
    for (const TilePos& pos : spread){
        const WallHit hit = walk_to_wall(world, pos, direction);
        if (!hit.anchored){
            return std::nullopt;
        }
        if (const std::optional<Stepped> stepped = step_tiles(world, pos, direction, (int)hit.tiles)){
            grass = std::max(grass, stepped->grass);
        }
        longest = std::max(longest, hit.tiles);
        expansion.next.push_back(hit.stop);
    }
    if (longest == 0){
        return std::nullopt;
    }
    expansion.cost = (long long)costs.per_leg + (long long)costs.per_tile * longest + (long long)costs.per_grass_tile * grass;
    expansion.leg = TileLeg{direction, longest, true};
    return expansion;
}

//  Open: about `tiles` tiles, give or take the error. Nothing if any
//  possible end leaves the maps or enters a door.
std::optional<Expansion> open_leg(const StoryWorld& world, const Spread& spread, Direction direction, int tiles, const RobustPlanCosts& costs){
    Expansion expansion;
    int grass = 0;
    const int error = tiles <= costs.exact_open_tiles ? 0 : costs.open_error + tiles / costs.open_error_per;
    for (const TilePos& pos : spread){
        for (int n = std::max(1, tiles - error); n <= tiles + error; n++){
            const std::optional<Stepped> stepped = step_tiles(world, pos, direction, n);
            if (!stepped){
                return std::nullopt;
            }
            grass = std::max(grass, stepped->grass);
            expansion.next.push_back(stepped->end);
        }
    }
    expansion.cost = (long long)costs.per_leg + costs.leg_not_at_wall +
        (long long)(costs.per_tile + costs.per_open_tile) * tiles + (long long)costs.per_grass_tile * grass;
    expansion.leg = TileLeg{direction, (size_t)tiles, false};
    return expansion;
}

//  Dijkstra over sets of tiles the player may stand on.
class RobustSearch{
public:
    RobustSearch(const StoryWorld& world, const RobustPlanCosts& costs)
        : m_world(world)
        , m_costs(costs)
    {}

    std::optional<std::vector<PlannedLeg>> run(Spread start, const std::function<bool(const TilePos&)>& goal){
        normalize(start);
        m_start = start;
        m_nodes[start] = Node{0, {}, {}};
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
                return legs_to(spread);
            }
            expand(spread, cost);
        }
        return std::nullopt;
    }

private:
    struct Node{
        long long cost = 0;
        Spread parent;
        PlannedLeg leg;
        bool done = false;
    };
    using Entry = std::pair<long long, Spread>;
    struct EntryGreater{
        bool operator()(const Entry& a, const Entry& b) const{ return a.first > b.first; }
    };

    void expand(const Spread& spread, long long cost){
        for (Direction direction : Pokemon::ALL_DIRECTIONS){
            if (std::optional<Expansion> wall = to_wall_leg(m_world, spread, direction, m_costs)){
                relax(spread, cost, std::move(*wall));
            }
            for (int tiles = 1; tiles <= m_costs.max_open_tiles; tiles++){
                std::optional<Expansion> open = open_leg(m_world, spread, direction, tiles, m_costs);
                if (!open){
                    break;
                }
                relax(spread, cost, std::move(*open));
            }
        }
    }

    //  Only the start may be wider than costs.max_spread: every leg must
    //  bring the player within it.
    void relax(const Spread& spread, long long cost, Expansion expansion){
        normalize(expansion.next);
        if (expansion.next.size() > m_costs.max_spread || expansion.next == spread){
            return;
        }
        const long long total = cost + expansion.cost;
        auto iter = m_nodes.find(expansion.next);
        if (iter != m_nodes.end() && iter->second.cost <= total){
            return;
        }
        m_nodes[expansion.next] = Node{total, spread, PlannedLeg{expansion.leg, expansion.next}};
        m_queue.emplace(total, std::move(expansion.next));
    }

    std::vector<PlannedLeg> legs_to(const Spread& end) const{
        std::vector<PlannedLeg> legs;
        for (Spread at = end; at != m_start; ){
            const Node& step = m_nodes.at(at);
            legs.push_back(step.leg);
            at = step.parent;
        }
        std::reverse(legs.begin(), legs.end());
        return legs;
    }

    const StoryWorld& m_world;
    const RobustPlanCosts& m_costs;
    Spread m_start;
    std::map<Spread, Node, SpreadLess> m_nodes;
    std::priority_queue<Entry, std::vector<Entry>, EntryGreater> m_queue;
};

}

std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const StoryWorld& world,
    const TilePos& from, const TilePos& to,
    const RobustPlanCosts& costs
){
    check_standable(world, from, to);
    return plan_robust_route(world, std::vector<TilePos>{from}, [&](const TilePos& pos){ return pos == to; }, costs);
}

std::optional<std::vector<PlannedLeg>> plan_robust_route(
    const StoryWorld& world,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal,
    const RobustPlanCosts& costs
){
    for (const TilePos& pos : from){
        if (!world.standable(pos)){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Route starts must be standable: " + Pokemon::to_string(pos) + ".");
        }
    }
    return RobustSearch(world, costs).run(from, goal);
}



}
}
}
