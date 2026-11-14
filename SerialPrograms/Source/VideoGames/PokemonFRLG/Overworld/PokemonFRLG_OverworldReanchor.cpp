/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Pokemon/Navigation/Pokemon_WallRecovery.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldResume.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr size_t MAX_HOLDS = 3;
//  How many of the route's steps after a leg walked on may go by before
//  they find the player.
constexpr size_t MAX_WALK_ON_STEPS = 4;

Direction opposite(Direction direction){
    return (Direction)(((int)direction + 2) % 4);
}

//  The way from `from` to `to`, on the same row or column.
Direction towards(const TilePos& from, const TilePos& to){
    if (to.x == from.x){
        return to.y < from.y ? Direction::NORTH : Direction::SOUTH;
    }
    return to.x < from.x ? Direction::WEST : Direction::EAST;
}

const MapData* map_named(const std::vector<MapData>& maps, const std::string& name){
    for (const MapData& map : maps){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Whether the player could stand on `position`.
bool standable(const std::vector<MapData>& maps, const MapPosition& position, bool surfing){
    const MapData* map = map_named(maps, position.map);
    return map != nullptr && in_bounds(*map, position.point)
        && !is_wall(*map, position.point) && object_at(*map, position.point) == nullptr
        && is_water(*map, position.point) == surfing;
}

template <typename Type>
void add_unique(std::vector<Type>& items, const Type& item){
    if (std::find(items.begin(), items.end(), item) == items.end()){
        items.push_back(item);
    }
}

//  Where `step` started: `tiles` back from its end, on the same map.
std::optional<MapPosition> walk_back(const std::vector<MapData>& maps, const RouteStep& step){
    MapPosition position = step.end;
    for (int c = 0; c < step.tiles; c++){
        position.point = position.point.next(opposite(step.direction));
        if (!standable(maps, position, step.surfing)){
            return std::nullopt;
        }
    }
    return position;
}

//  Where trainer `object` of `map`, who may see the player on `tile`,
//  stands once walked up: next to the player, on the side they came from.
//  Each tile a wanderer may stand on is checked on its own, since the side
//  depends on it.
void add_trainer_stops(
    const MapData& map, size_t object, const MapPosition& tile, std::vector<TrainerStop>& stops
){
    const MapObject& trainer = map.objects[object];
    const Pokemon::TileBox box = trainer.wander.value_or(
        Pokemon::TileBox{.x0 = trainer.x, .y0 = trainer.y, .x1 = trainer.x, .y1 = trainer.y}
    );
    const int width = box.x1 - box.x0 + 1;
    const int count = width * (box.y1 - box.y0 + 1);
    for (int c = 0; c < count; c++){
        MapObject standing = trainer;
        standing.wander.reset();
        standing.x = box.x0 + c % width;
        standing.y = box.y0 + c / width;
        if (trainer_may_see(map, standing, tile.point)){
            const TilePos beside = tile.point.next(towards(tile.point, TilePos{standing.x, standing.y}));
            add_unique(stops, TrainerStop{.object = object, .at = MapPosition{tile.map, beside}});
        }
    }
}

//  The trainers who may see the player on `tile`, and where each stands
//  once walked up.
std::vector<TrainerStop> trainers_seeing(const std::vector<MapData>& maps, const MapPosition& tile){
    std::vector<TrainerStop> stops;
    const MapData* map = map_named(maps, tile.map);
    for (size_t c = 0; map != nullptr && c < map->objects.size(); c++){
        if (trainer_may_see(*map, map->objects[c], tile.point)){
            add_trainer_stops(*map, c, tile, stops);
        }
    }
    return stops;
}

//  `maps` with a trainer who walked up standing on each of `trainers`: they
//  stay there after the battle, so the player can't walk through.
std::vector<MapData> with_trainers(const std::vector<MapData>& maps, const std::vector<MapPosition>& trainers){
    std::vector<MapData> ret = maps;
    for (const MapPosition& at : trainers){
        for (MapData& map : ret){
            if (map.name == at.map && in_bounds(map, at.point)){
                map.tiles.cells[(size_t)at.point.y * map.tiles.width + at.point.x] = TileKind::WALL;
            }
        }
    }
    return ret;
}

//  Where the player may be, on which of the maps as they may stand (with
//  or without a trainer next to them).
struct Hypothesis{
    size_t world = 0;
    MapPosition at;

    bool operator==(const Hypothesis& other) const = default;
};

//  Whether a person stands, or may wander, on a tile `wall` passes after
//  `from`. The step rules don't have people, but one there stops the player
//  short of the wall.
bool person_on_the_way(
    const std::vector<MapData>& world, const MapPosition& from,
    bool surfing, Direction direction, const WallHold& wall
){
    RouteStep hold;
    hold.direction = direction;
    hold.tiles = wall.tiles;
    hold.surfing = surfing;
    const std::vector<MapPosition> tiles = leg_tiles(world, from, hold);
    for (size_t c = 1; c < tiles.size(); c++){
        const MapData* map = map_named(world, tiles[c].map);
        if (map != nullptr && object_at(*map, tiles[c].point) != nullptr){
            return true;
        }
    }
    return false;
}

//  Holding `direction` from each of `positions`: where they end up, as a leg
//  to a wall as long as the longest of them. Nothing if any isn't stopped
//  by a wall, or may be stopped short by a person.
std::optional<std::pair<std::vector<Hypothesis>, RouteStep>> hold_all(
    const std::vector<std::vector<MapData>>& worlds, const std::vector<Hypothesis>& positions,
    bool surfing, Direction direction
){
    std::vector<Hypothesis> ends;
    RouteStep hold;
    hold.direction = direction;
    hold.surfing = surfing;
    hold.to_wall = true;
    hold.facing_after = direction;
    for (const Hypothesis& position : positions){
        const std::optional<WallHold> wall = hold_to_wall(worlds[position.world], position.at, surfing, direction);
        if (!wall || person_on_the_way(worlds[position.world], position.at, surfing, direction, *wall)){
            return std::nullopt;
        }
        add_unique(ends, Hypothesis{position.world, wall->end});
        hold.tiles = std::max(hold.tiles, wall->tiles);
        hold.wild = hold.wild || wall->wild;
        hold.walk_only = hold.walk_only || wall->walk_only;
        hold.end = wall->end;
    }
    return std::make_pair(std::move(ends), hold);
}

using Found = Pokemon::WallAnchor<MapPosition, RouteStep>;

//  Runtime world alternatives and movement mode, separate from the policy type.
struct ReanchorContext{
    const std::vector<std::vector<MapData>>& worlds;
    bool surfing = false;
};

class ReanchorPolicy{
public:
    using Hypothesis = PokemonFRLG::Hypothesis;
    using Position = MapPosition;
    using Hold = RouteStep;

    static Position position(const Hypothesis& hypothesis){ return hypothesis.at; }
    static int tiles(const Hold& hold){ return hold.tiles; }
    static std::optional<Pokemon::WallRecoveryStep<Hypothesis, Hold>> hold(
        const ReanchorContext& context, const std::vector<Hypothesis>& positions, Direction direction
    ){
        auto next = hold_all(context.worlds, positions, context.surfing, direction);
        if (!next){
            return std::nullopt;
        }
        return Pokemon::WallRecoveryStep<Hypothesis, Hold>{std::move(next->first), std::move(next->second)};
    }

};

//  Whether `route`, planned from `at` on other maps, walks the same on
//  `world`: each leg to a wall stops at its end, each open leg gets there.
bool walks_same(const std::vector<MapData>& world, MapPosition at, const std::vector<RouteStep>& route){
    for (const RouteStep& step : route){
        if (step.action == RouteAction::WALK && step.to_wall){
            const std::optional<WallHold> hold = hold_to_wall(world, at, step.surfing, step.direction);
            if (!hold || !(hold->end == step.end)){
                return false;
            }
        }else if (step.action == RouteAction::WALK){
            const std::vector<MapPosition> tiles = leg_tiles(world, at, step);
            const size_t expected = (size_t)(step.tiles - (step.warps ? 1 : 0)) + 1;
            if (tiles.size() != expected || (!step.warps && !(tiles.back() == step.end))){
                return false;
            }
        }
        at = step.end;
    }
    return true;
}

bool walks_same_everywhere(
    const std::vector<std::vector<MapData>>& worlds, const std::vector<Hypothesis>& positions,
    const MapPosition& from, const std::vector<RouteStep>& route
){
    for (const Hypothesis& position : positions){
        if (!walks_same(worlds[position.world], from, route)){
            return false;
        }
    }
    return true;
}

int held_tiles(const std::vector<RouteStep>& holds){
    int tiles = 0;
    for (const RouteStep& hold : holds){
        tiles += hold.tiles;
    }
    return tiles;
}

//  The ways to find the player from all of `positions`, fewest holds first,
//  then fewest tiles held. None needed if they are all on one tile. Not
//  only the fewest holds: for each first hold, the fewest after it, so a
//  caller can pick a way that keeps what a stopped leg walked. On Rock
//  Tunnel B1F row 37 the one single hold went back east to the wall
//  behind the leg's start, while a hold south and one west, ahead, were
//  found too (console B, 2026-11-09).
std::vector<Found> find_player(
    const std::vector<std::vector<MapData>>& worlds, const std::vector<Hypothesis>& positions, bool surfing
){
    const ReanchorContext context{.worlds = worlds, .surfing = surfing};
    std::vector<Found> found = Pokemon::find_wall_anchors<ReanchorPolicy>(positions, context, {.max_holds = 0});
    if (!found.empty()){
        return found;
    }
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
        const auto first = ReanchorPolicy::hold(context, positions, direction);
        if (!first || first->positions == positions || first->hold.tiles == 0){
            continue;
        }
        for (Found& rest : Pokemon::find_wall_anchors<ReanchorPolicy>(first->positions, context, {.max_holds = MAX_HOLDS - 1})){
            rest.holds.insert(rest.holds.begin(), first->hold);
            found.push_back(std::move(rest));
        }
    }
    std::stable_sort(found.begin(), found.end(), [](const Found& a, const Found& b){
        if (a.holds.size() != b.holds.size()){
            return a.holds.size() < b.holds.size();
        }
        return held_tiles(a.holds) < held_tiles(b.holds);
    });
    return found;
}

} // namespace


WalkedTiles walked_near(const RouteStep& step, int estimate, Milliseconds tile_time){
    //  The estimate is rounded to the nearest tile already.
    const int spread = tile_time.count() > 0
        ? (int)((STOP_TIMING_SLACK + tile_time / 2) / tile_time)
        : step.tiles;
    return WalkedTiles{
        .min = std::clamp(estimate - spread, 0, step.tiles),
        .max = std::clamp(estimate + spread, 0, step.tiles),
    };
}

std::vector<RouteStep> with_warp_slack(std::vector<RouteStep> route){
    const auto same_axis = [](Direction a, Direction b){ return (int)a % 2 == (int)b % 2; };
    std::optional<Direction> further;
    for (RouteStep& step : route){
        if (step.action != RouteAction::WALK){
            continue;
        }
        if (step.warps){
            if (further && step.direction != *further && same_axis(step.direction, *further)){
                step.warp_slack = 1;
            }
            further.reset();
            continue;
        }
        if (step.to_wall && further && same_axis(step.direction, *further)){
            further.reset();
        }
        if (!step.to_wall && !step.connection_steps.empty()){
            further = step.direction;
        }
    }
    return route;
}

WalkedTiles whole_leg(const RouteStep& step){
    return WalkedTiles{.min = 0, .max = step.tiles};
}

WalkedTiles stop_tiles(const RouteStep& step, WalkedTiles timed, bool dark){
    return dark ? whole_leg(step) : timed;
}

std::string to_string(const StopCandidate& candidate){
    std::string text = to_string(candidate.player);
    if (candidate.trainer){
        text += std::format(" (stopped by a trainer, now at {})", Pokemon::to_string(candidate.trainer->at.point));
    }
    return text;
}

std::vector<StopCandidate> stop_candidates(
    const std::vector<MapData>& maps,
    const std::optional<MapPosition>& start,
    const RouteStep& step,
    WalkedTiles walked
){
    std::vector<StopCandidate> candidates;
    if (step.action != RouteAction::WALK){
        return candidates;
    }
    const std::optional<MapPosition> from = start ? start : walk_back(maps, step);
    if (!from){
        return candidates;
    }
    const std::vector<MapPosition> tiles = leg_tiles(maps, *from, step);
    const int last = std::min((int)tiles.size() - 1, walked.max);
    for (int c = 1; c <= last; c++){
        for (const TrainerStop& trainer : trainers_seeing(maps, tiles[c])){
            candidates.push_back(StopCandidate{.player = tiles[c], .trainer = trainer});
        }
    }
    if (!candidates.empty() && !step.wild){
        return candidates;
    }
    for (int c = std::max(0, walked.min); c <= last; c++){
        add_unique(candidates, StopCandidate{.player = tiles[c], .trainer = std::nullopt});
    }
    //  A leg onto a spinner that took all its tiles slid on to its stop
    //  (09.02 run 8, console B, 2026-11-11: a tap onto B2F's arrow (16, 6)
    //  read as not moved).
    if (step.spin_tiles && walked.max >= step.tiles){
        add_unique(candidates, StopCandidate{.player = step.end, .trainer = std::nullopt});
    }
    return candidates;
}

int reanchor_tiles(const Reanchor& anchor){
    return held_tiles(anchor.holds) + held_tiles(anchor.back);
}

namespace{

//  How much more a tile of an open leg costs than one held to a wall: a
//  wild battle on a hold costs only the battle, as the hold goes on to its
//  wall, while one on an open leg stops it and needs finding the player
//  again. Not measured: weighted so that on Rock Tunnel B1F row 38 the way
//  west to the wall (1, 38) and 13 tiles back beats 10 held east to the
//  wall (30, 38) and 17 back.
constexpr int OPEN_TILE_WEIGHT = 4;

//  How costly `anchor` is to walk: its tiles, with open legs' weighted.
int reanchor_cost(const Reanchor& anchor){
    int open = 0;
    for (const RouteStep& step : anchor.back){
        if (step.action == RouteAction::WALK && !step.to_wall && !step.warps){
            open += step.tiles;
        }
    }
    return reanchor_tiles(anchor) + (OPEN_TILE_WEIGHT - 1) * open;
}

//  Whether `holds`, from any of `positions`, pass a tile a trainer may see.
//  A trainer the route keeps clear of would stop the player and fight
//  (the sim, 2026-11-09: Hiker Lucas, on a hold down Rock Tunnel 1F's
//  column 16 after a wild battle below the ladder (20, 13)).
bool holds_in_sight(
    const std::vector<std::vector<MapData>>& worlds, const std::vector<Hypothesis>& positions, const std::vector<RouteStep>& holds
){
    for (const Hypothesis& position : positions){
        const std::vector<MapData>& world = worlds[position.world];
        MapPosition at = position.at;
        for (const RouteStep& hold : holds){
            const std::optional<WallHold> wall = hold_to_wall(world, at, hold.surfing, hold.direction);
            if (!wall){
                break;
            }
            RouteStep held = hold;
            held.tiles = wall->tiles;
            for (const MapPosition& tile : leg_tiles(world, at, held)){
                if (!trainers_seeing(world, tile).empty()){
                    return true;
                }
            }
            at = wall->end;
        }
    }
    return false;
}

//  Ways whose holds pass a trainer's sight come after every other way.
constexpr int IN_SIGHT_COST = 1000;

//  The maps as they may stand after a stop on one of `candidates`: as they
//  are, and with each trainer who may have walked up; where those trainers
//  stand, and where the player may be on which of the maps.
struct StopWorlds{
    std::vector<std::vector<MapData>> worlds;
    std::vector<MapPosition> trainers;
    std::vector<Hypothesis> positions;
};
StopWorlds stop_worlds(const std::vector<MapData>& maps, const std::vector<StopCandidate>& candidates){
    StopWorlds out{.worlds = {maps}, .trainers = {}, .positions = {}};
    for (const StopCandidate& candidate : candidates){
        size_t world = 0;
        if (candidate.trainer){
            out.worlds.push_back(with_trainers(maps, {candidate.trainer->at}));
            world = out.worlds.size() - 1;
            add_unique(out.trainers, candidate.trainer->at);
        }
        add_unique(out.positions, Hypothesis{world, candidate.player});
    }
    return out;
}

} // namespace

std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const RouteCosts& costs
){
    if (candidates.empty() || step.action != RouteAction::WALK){
        return std::nullopt;
    }

    const StopWorlds stop = stop_worlds(maps, candidates);
    const std::vector<std::vector<MapData>>& worlds = stop.worlds;
    const std::vector<MapPosition>& trainers = stop.trainers;
    const std::vector<Hypothesis>& positions = stop.positions;

    //  Of the ways found, the cheapest (reanchor_cost()): it keeps the most
    //  of what the leg walked. A wild battle every 10 to 14 tiles stopped
    //  every try of a leg whose way back started behind it (console B,
    //  2026-11-09, Rock Tunnel B1F row 37).
    const std::vector<MapData> clear_of_trainers = with_trainers(maps, trainers);
    std::optional<Reanchor> best;
    int best_cost = 0;
    for (const Found& at : find_player(worlds, positions, step.surfing)){
        const Direction facing = at.holds.empty() ? step.direction : at.holds.back().direction;
        //  The way back keeps clear of every trainer who may stand in it if
        //  it can, and must walk the same whether they do or not.
        std::optional<std::vector<RouteStep>> back = plan_overworld_route(clear_of_trainers, at.at, RouteGoal{step.end, std::nullopt}, costs, facing);
        if (!back && !trainers.empty()){
            back = plan_overworld_route(maps, at.at, RouteGoal{step.end, std::nullopt}, costs, facing);
        }
        if (!back || (!trainers.empty() && !walks_same_everywhere(worlds, positions, at.at, *back))){
            continue;
        }
        Reanchor anchor{.holds = at.holds, .found = at.at, .back = std::move(*back)};
        const int cost = reanchor_cost(anchor) + (holds_in_sight(worlds, positions, anchor.holds) ? IN_SIGHT_COST : 0);
        if (!best || cost < best_cost){
            best = std::move(anchor);
            best_cost = cost;
        }
    }
    return best;
}

namespace{

//  How many tiles `from` is before `to`, walking `direction` on one map;
//  nothing if `to` isn't ahead on that line within `max_tiles`.
std::optional<int> tiles_ahead(const MapPosition& from, const MapPosition& to, Direction direction, int max_tiles){
    if (from.map != to.map){
        return std::nullopt;
    }
    TilePos at = from.point;
    for (int c = 0; c <= max_tiles; c++){
        if (at == to.point){
            return c;
        }
        at = at.next(direction);
    }
    return std::nullopt;
}

//  One candidate's way on: the tiles it walks, and where it stands.
struct WayOn{
    std::vector<MapPosition> tiles;
    MapPosition at;
};

//  `step`, a hold to a wall or an open leg onto no warp, from where each of
//  `ways` stands. False if a hold isn't stopped by a wall, an open leg is
//  stopped short, or either may be stopped short by a person.
bool walk_ways(const std::vector<MapData>& maps, const RouteStep& step, std::vector<WayOn>& ways){
    if (step.action != RouteAction::WALK || step.warps || !step.connection_steps.empty()){
        return false;
    }
    for (WayOn& way : ways){
        std::optional<WallHold> walked = step.to_wall
            ? hold_to_wall(maps, way.at, step.surfing, step.direction)
            : std::optional<WallHold>(WallHold{.end = way.at, .tiles = step.tiles});
        if (!walked || person_on_the_way(maps, way.at, step.surfing, step.direction, *walked)){
            return false;
        }
        RouteStep held = step;
        held.tiles = walked->tiles;
        const std::vector<MapPosition> tiles = leg_tiles(maps, way.at, held);
        if (tiles.size() != (size_t)held.tiles + 1){
            return false;
        }
        way.tiles.insert(way.tiles.end(), tiles.begin() + 1, tiles.end());
        way.at = tiles.back();
    }
    return true;
}

//  Whether any of `ways` walks onto a tile off `planned` that a trainer may
//  see: walking on a few tiles off the planned way may cross a trainer's
//  line the route keeps clear of.
bool off_way_in_sight(const std::vector<MapData>& maps, const std::vector<WayOn>& ways, const std::vector<MapPosition>& planned){
    for (const WayOn& way : ways){
        for (const MapPosition& tile : way.tiles){
            const bool off_way = std::find(planned.begin(), planned.end(), tile) == planned.end();
            if (off_way && !trainers_seeing(maps, tile).empty()){
                return true;
            }
        }
    }
    return false;
}

//  Each of `candidates` walking `rest`; nothing if any is stopped short.
std::optional<std::vector<WayOn>> walk_rest(
    const std::vector<MapData>& maps, const std::vector<StopCandidate>& candidates, const RouteStep& rest
){
    std::vector<WayOn> ways;
    for (const StopCandidate& candidate : candidates){
        std::vector<MapPosition> tiles = leg_tiles(maps, candidate.player, rest);
        if (tiles.size() != (size_t)rest.tiles + 1){
            return std::nullopt;
        }
        const MapPosition at = tiles.back();
        ways.push_back(WayOn{std::move(tiles), at});
    }
    return ways;
}

} // namespace

std::optional<Resume> plan_resume(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const std::vector<RouteStep>& after
){
    const bool open_leg = step.action == RouteAction::WALK && !step.to_wall && !step.warps && step.connection_steps.empty();
    const bool by_trainer = std::any_of(candidates.begin(), candidates.end(), [](const StopCandidate& c){ return c.trainer.has_value(); });
    if (candidates.size() < 2 || !open_leg || by_trainer){
        return std::nullopt;
    }
    const std::optional<int> remaining = tiles_ahead(candidates[candidates.size() / 2].player, step.end, step.direction, step.tiles);
    if (!remaining){
        return std::nullopt;
    }
    Resume resume{.rest = step, .steps = 0, .starts = {{}}};
    resume.rest.tiles = *remaining;
    for (const StopCandidate& candidate : candidates){
        resume.starts[0].push_back(candidate.player);
    }
    std::optional<std::vector<WayOn>> ways = walk_rest(maps, candidates, resume.rest);
    //  The way as planned, from the leg's end.
    std::vector<WayOn> planned{WayOn{{step.end}, step.end}};
    for (size_t c = 0; ways && c < after.size() && c < MAX_WALK_ON_STEPS; c++){
        resume.starts.emplace_back();
        for (const WayOn& way : *ways){
            resume.starts.back().push_back(way.at);
        }
        if (!walk_ways(maps, after[c], *ways) || !walk_ways(maps, after[c], planned)){
            return std::nullopt;
        }
        const bool found = after[c].to_wall && std::all_of(ways->begin(), ways->end(), [&](const WayOn& way){ return way.at == after[c].end; });
        if (found){
            resume.steps = c + 1;
            return off_way_in_sight(maps, *ways, planned[0].tiles) ? std::nullopt : std::optional<Resume>(std::move(resume));
        }
    }
    return std::nullopt;
}

std::optional<std::vector<RouteStep>> holds_to_one_tile(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& tiles
){
    std::vector<Hypothesis> positions;
    for (const MapPosition& tile : tiles){
        add_unique(positions, Hypothesis{0, tile});
    }
    if (positions.empty()){
        return std::nullopt;
    }
    const std::vector<Found> found = find_player({maps}, positions, false);
    if (found.empty()){
        return std::nullopt;
    }
    return found[0].holds;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
