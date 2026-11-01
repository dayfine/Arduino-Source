/*  Overworld Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<Tile> tile_from_char(char c){
    switch (c){
    case '#': return Tile::WALL;
    case 'J': return Tile::LEDGE;
    case '.': return Tile::FLOOR;
    case '"': return Tile::TALL_GRASS;
    case '~': return Tile::WATER;
    case 'D': return Tile::DOOR;
    case 'S': return Tile::SIGN;
    case 'C': return Tile::CAVE_ENTRANCE;
    case 'L': return Tile::LADDER;
    case '^': return Tile::MAT_NORTH;
    case 'v': return Tile::MAT_SOUTH;
    case '<': return Tile::MAT_WEST;
    case '>': return Tile::MAT_EAST;
    case 'n': return Tile::CLOSED_NORTH;
    case 's': return Tile::CLOSED_SOUTH;
    case 'e': return Tile::CLOSED_EAST;
    case 'w': return Tile::CLOSED_WEST;
    default: return std::nullopt;
    }
}


bool in_bounds(const MapData& map, const TilePos& point){
    return map.tiles.contains(point);
}
Tile tile_at(const MapData& map, const TilePos& point){
    return in_bounds(map, point) ? map.tiles.at(point) : Tile::WALL;
}
uint8_t elevation_at(const MapData& map, const TilePos& point){
    return map.elevation.contains(point) ? map.elevation.at(point) : ELEVATION_IGNORED;
}

bool is_wall(const MapData& map, const TilePos& point){
    const Tile tile = tile_at(map, point);
    return tile == Tile::WALL || tile == Tile::DOOR || tile == Tile::SIGN || tile == Tile::LEDGE;
}
bool is_ledge(const MapData& map, const TilePos& point){
    return tile_at(map, point) == Tile::LEDGE;
}
bool is_water(const MapData& map, const TilePos& point){
    return tile_at(map, point) == Tile::WATER;
}
bool is_tall_grass(const MapData& map, const TilePos& point){
    return tile_at(map, point) == Tile::TALL_GRASS;
}

namespace{

//  The tile closed on its `side` edge.
Tile closed_side(Direction side){
    switch (side){
    case Direction::NORTH:
        return Tile::CLOSED_NORTH;
    case Direction::SOUTH:
        return Tile::CLOSED_SOUTH;
    case Direction::EAST:
        return Tile::CLOSED_EAST;
    case Direction::WEST:
        return Tile::CLOSED_WEST;
    }
    return Tile::WALL;
}
Direction opposite(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return Direction::SOUTH;
    case Direction::SOUTH:
        return Direction::NORTH;
    case Direction::EAST:
        return Direction::WEST;
    case Direction::WEST:
        return Direction::EAST;
    }
    return direction;
}

}

bool blocks_entering(const MapData& map, const TilePos& point, Direction direction){
    //  Moving south enters by the north edge.
    return tile_at(map, point) == closed_side(opposite(direction));
}
bool blocks_leaving(const MapData& map, const TilePos& point, Direction direction){
    return tile_at(map, point) == closed_side(direction);
}

bool warps_on_step(const MapData& map, const TilePos& point){
    const Tile tile = tile_at(map, point);
    return tile == Tile::CAVE_ENTRANCE || tile == Tile::LADDER;
}
bool is_door(const MapData& map, const TilePos& point){
    return tile_at(map, point) == Tile::DOOR;
}
std::optional<Direction> exit_mat(const MapData& map, const TilePos& point){
    switch (tile_at(map, point)){
    case Tile::MAT_NORTH:
        return Direction::NORTH;
    case Tile::MAT_SOUTH:
        return Direction::SOUTH;
    case Tile::MAT_WEST:
        return Direction::WEST;
    case Tile::MAT_EAST:
        return Direction::EAST;
    default:
        return std::nullopt;
    }
}
bool walks_out_on_arrival(const MapData& map, const TilePos& point){
    const Tile tile = tile_at(map, point);
    return tile == Tile::DOOR || tile == Tile::CAVE_ENTRANCE;
}

void hide_objects(MapData& map, const std::set<std::string>& flags){
    std::vector<MapObject> shown;
    for (const MapObject& object : map.objects){
        if (object.flag.empty() || flags.count(object.flag) == 0){
            shown.push_back(object);
        }
    }
    map.objects = std::move(shown);
}

const MapObject* object_at(const MapData& map, const TilePos& point){
    for (const MapObject& object : map.objects){
        if (std::abs(object.x - point.x) <= object.wander_x && std::abs(object.y - point.y) <= object.wander_y){
            return &object;
        }
    }
    return nullptr;
}
const MapWarp* warp_at(const MapData& map, const TilePos& point){
    for (const MapWarp& warp : map.warps){
        if (warp.x == point.x && warp.y == point.y){
            return &warp;
        }
    }
    return nullptr;
}



StepKind classify_step(const MapData& map, const PlayerState& state, Direction direction){
    return classify_step_onto(map, state, direction, map, state.position.next(direction));
}

StepKind classify_step_onto(
    const MapData& from, const PlayerState& state, Direction direction,
    const MapData& to, const TilePos& destination
){
    if (is_wall(to, destination)
        || blocks_leaving(from, state.position, direction)
        || blocks_entering(to, destination, direction)
    ){
        return StepKind::BLOCKED;
    }

    const uint8_t level = elevation_at(to, destination);
    const bool mismatch = state.elevation != ELEVATION_ANY
        && level != ELEVATION_ANY && level != ELEVATION_IGNORED
        && level != state.elevation;
    if (mismatch){
        //  CanStopSurfing() and IsPlayerFacingSurfableFishableWater().
        if (state.surfing && level == ELEVATION_GROUND && object_at(to, destination) == nullptr){
            return StepKind::STOP_SURFING;
        }
        if (!state.surfing && state.elevation == ELEVATION_GROUND && is_water(to, destination)){
            return StepKind::START_SURF;
        }
        return StepKind::BLOCKED;
    }

    if (object_at(to, destination) != nullptr){
        return StepKind::BLOCKED;
    }
    return StepKind::WALK;
}

PlayerState after_step(const MapData& map, const PlayerState& state, Direction direction, StepKind kind){
    return after_step_onto(map, state, map, state.position.next(direction), kind);
}

PlayerState after_step_onto(
    const MapData& from, const PlayerState& state,
    const MapData& to, const TilePos& destination, StepKind kind
){
    PlayerState next = state;
    next.position = destination;
    const uint8_t level = elevation_at(to, destination);
    if (level != ELEVATION_IGNORED && elevation_at(from, state.position) != ELEVATION_IGNORED){
        next.elevation = level;
    }
    if (kind == StepKind::START_SURF){
        next.surfing = true;
    }else if (kind == StepKind::STOP_SURFING){
        next.surfing = false;
    }
    return next;
}



}
}
}
