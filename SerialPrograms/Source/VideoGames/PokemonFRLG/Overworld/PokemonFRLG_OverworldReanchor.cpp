/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Pokemon/Navigation/Pokemon_WallRecovery.h"
#include "PokemonFRLG_OverworldReanchor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr size_t MAX_HOLDS = 3;

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

//  The ways to find the player from all of `positions`: fewest holds first,
//  then fewest tiles held. None needed if they are all on one tile.
std::vector<Found> find_player(
    const std::vector<std::vector<MapData>>& worlds, const std::vector<Hypothesis>& positions, bool surfing
){
    return Pokemon::find_wall_anchors<ReanchorPolicy>(
        positions, ReanchorContext{.worlds = worlds, .surfing = surfing}, {.max_holds = MAX_HOLDS}
    );
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
    return candidates;
}

std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::vector<StopCandidate>& candidates,
    const RouteStep& step,
    const RouteCosts& costs
){
    if (candidates.empty() || step.action != RouteAction::WALK){
        return std::nullopt;
    }

    //  The maps as they may stand: as they are, and with each trainer who
    //  may have walked up.
    std::vector<std::vector<MapData>> worlds{maps};
    std::vector<MapPosition> trainers;
    std::vector<Hypothesis> positions;
    for (const StopCandidate& candidate : candidates){
        size_t world = 0;
        if (candidate.trainer){
            worlds.push_back(with_trainers(maps, {candidate.trainer->at}));
            world = worlds.size() - 1;
            add_unique(trainers, candidate.trainer->at);
        }
        add_unique(positions, Hypothesis{world, candidate.player});
    }

    const std::vector<MapData> clear_of_trainers = with_trainers(maps, trainers);
    for (const Found& at : find_player(worlds, positions, step.surfing)){
        const Direction facing = at.holds.empty() ? step.direction : at.holds.back().direction;
        //  The way back keeps clear of every trainer who may stand in it if
        //  it can, and must walk the same whether they do or not.
        std::optional<std::vector<RouteStep>> back = plan_overworld_route(clear_of_trainers, at.at, RouteGoal{step.end, std::nullopt}, costs, facing);
        if (!back && !trainers.empty()){
            back = plan_overworld_route(maps, at.at, RouteGoal{step.end, std::nullopt}, costs, facing);
        }
        if (back && (trainers.empty() || walks_same_everywhere(worlds, positions, at.at, *back))){
            return Reanchor{.holds = at.holds, .found = at.at, .back = std::move(*back)};
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
