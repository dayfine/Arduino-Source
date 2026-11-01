/*  Overworld Re-anchor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "PokemonFRLG_OverworldReanchor.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  How far to the side of a leg the player may be: an earlier open leg can
//  leave them tiles off (console B, 2026-11-01, Viridian Forest: "west 6"
//  walked 2 of its 6 tiles, unseen, and the next leg went up a column 4
//  tiles over).
constexpr int MAX_SPREAD = 4;
constexpr size_t MAX_HOLDS = 3;

Direction opposite(Direction direction){
    return (Direction)(((int)direction + 2) % 4);
}
Direction across(Direction direction){
    return (Direction)(((int)direction + 1) % 4);
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

void add_unique(std::vector<MapPosition>& positions, const MapPosition& position){
    if (std::find(positions.begin(), positions.end(), position) == positions.end()){
        positions.push_back(position);
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

//  Every tile the player may be on: along the leg, and up to `spread` to
//  either side of it.
std::vector<MapPosition> candidates(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step, int spread
){
    std::vector<MapPosition> positions;
    for (const MapPosition& tile : leg_tiles(maps, start, step)){
        for (int side = -spread; side <= spread; side++){
            MapPosition position = tile;
            for (int c = 0; c < std::abs(side); c++){
                position.point = position.point.next(side < 0 ? opposite(across(step.direction)) : across(step.direction));
            }
            if (standable(maps, position, step.surfing)){
                add_unique(positions, position);
            }
        }
    }
    return positions;
}

//  Holding `direction` from each of `positions`: where they end up, as a leg
//  to a wall as long as the longest of them. Nothing if any isn't stopped
//  by a wall.
std::optional<std::pair<std::vector<MapPosition>, RouteStep>> hold_all(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& positions,
    bool surfing, Direction direction
){
    std::vector<MapPosition> ends;
    RouteStep hold;
    hold.direction = direction;
    hold.surfing = surfing;
    hold.to_wall = true;
    hold.facing_after = direction;
    for (const MapPosition& position : positions){
        const std::optional<WallHold> wall = hold_to_wall(maps, position, surfing, direction);
        if (!wall){
            return std::nullopt;
        }
        add_unique(ends, wall->end);
        hold.tiles = std::max(hold.tiles, wall->tiles);
        hold.wild = hold.wild || wall->wild;
        hold.end = wall->end;
    }
    return std::make_pair(std::move(ends), hold);
}

int total_tiles(const std::vector<RouteStep>& legs){
    int total = 0;
    for (const RouteStep& leg : legs){
        total += leg.tiles;
    }
    return total;
}

//  Up to `depth` holds that bring all of `positions` to one tile; the one
//  with the fewest tiles held.
void search(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& positions, bool surfing,
    size_t depth, std::vector<RouteStep>& holds, std::optional<Reanchor>& best
){
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
        auto next = hold_all(maps, positions, surfing, direction);
        if (!next || next->first == positions || next->second.tiles == 0){
            continue;
        }
        holds.push_back(next->second);
        if (next->first.size() == 1){
            if (!best || total_tiles(holds) < total_tiles(best->holds)){
                best = Reanchor{holds, next->first[0]};
            }
        }else if (depth > 1){
            search(maps, next->first, surfing, depth - 1, holds, best);
        }
        holds.pop_back();
    }
}

//  Held toward a sign moving north or south, the player stops beside it and
//  it speaks: if only one of `positions` is beside one, that's where they are.
std::optional<MapPosition> beside_sign(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& positions, Direction direction
){
    if (direction != Direction::NORTH && direction != Direction::SOUTH){
        return std::nullopt;
    }
    std::optional<MapPosition> found;
    for (const MapPosition& position : positions){
        const MapData* map = map_named(maps, position.map);
        if (map != nullptr && tile_at(*map, position.point.next(direction)) == Tile::SIGN){
            if (found){
                return std::nullopt;
            }
            found = position;
        }
    }
    return found;
}

}


std::optional<Reanchor> plan_reanchor(
    const std::vector<MapData>& maps,
    const std::optional<MapPosition>& start,
    const RouteStep& step,
    bool at_sign
){
    if (step.action != RouteAction::WALK){
        return std::nullopt;
    }
    //  A leg to a wall may have been held from anywhere along its line.
    const std::optional<MapPosition> from = start && !step.to_wall ? start : walk_back(maps, step);
    if (!from){
        return std::nullopt;
    }
    for (int spread = MAX_SPREAD; spread >= 0; spread--){
        const std::vector<MapPosition> positions = candidates(maps, *from, step, spread);
        //  Only with every tile counted: a sign beside one of fewer could be
        //  the wrong sign.
        if (at_sign && spread == MAX_SPREAD){
            if (const std::optional<MapPosition> beside = beside_sign(maps, positions, step.direction)){
                return Reanchor{{}, *beside};
            }
        }
        for (size_t depth = 1; depth <= MAX_HOLDS; depth++){
            std::vector<RouteStep> holds;
            std::optional<Reanchor> best;
            search(maps, positions, step.surfing, depth, holds, best);
            if (best){
                return best;
            }
        }
    }
    return std::nullopt;
}



}
}
}
