/*  Overworld Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


bool in_bounds(const MapData& map, const TilePos& point){
    return map.tiles.contains(point);
}
TileKind tile_at(const MapData& map, const TilePos& point){
    return in_bounds(map, point) ? map.tiles.at(point) : TileKind::WALL;
}
uint8_t elevation_at(const MapData& map, const TilePos& point){
    if (!in_bounds(map, point)){
        return ELEVATION_IGNORED;
    }
    return map.elevation ? map.elevation->at(point) : ELEVATION_GROUND;
}

bool is_wall(const MapData& map, const TilePos& point){
    const TileKind tile = tile_at(map, point);
    return tile == TileKind::WALL || tile == TileKind::DOOR || tile == TileKind::SIGN || is_ledge(map, point);
}
bool is_ledge(const MapData& map, const TilePos& point){
    switch (tile_at(map, point)){
    case TileKind::LEDGE_NORTH:
    case TileKind::LEDGE_SOUTH:
    case TileKind::LEDGE_WEST:
    case TileKind::LEDGE_EAST:
        return true;
    default:
        return false;
    }
}
bool is_water(const MapData& map, const TilePos& point){
    return tile_at(map, point) == TileKind::WATER;
}
bool is_tall_grass(const MapData& map, const TilePos& point){
    return tile_at(map, point) == TileKind::TALL_GRASS;
}

namespace{

//  The tile closed on its `side` edge.
TileKind closed_side(Direction side){
    switch (side){
    case Direction::NORTH:
        return TileKind::CLOSED_NORTH;
    case Direction::SOUTH:
        return TileKind::CLOSED_SOUTH;
    case Direction::EAST:
        return TileKind::CLOSED_EAST;
    case Direction::WEST:
        return TileKind::CLOSED_WEST;
    }
    return TileKind::WALL;
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

//  How the warp on this tile is entered, if there is one.
std::optional<WarpEntry> warp_entry(const MapData& map, const TilePos& point){
    const MapWarp* warp = warp_at(map, point);
    if (warp == nullptr){
        return std::nullopt;
    }
    return warp->enter;
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
    const std::optional<WarpEntry> entry = warp_entry(map, point);
    return entry == WarpEntry::STEP_ON || entry == WarpEntry::CAVE_ENTRANCE;
}
bool is_door(const MapData& map, const TilePos& point){
    return tile_at(map, point) == TileKind::DOOR;
}
std::optional<Direction> exit_mat(const MapData& map, const TilePos& point){
    const std::optional<WarpEntry> entry = warp_entry(map, point);
    if (!entry){
        return std::nullopt;
    }
    switch (*entry){
    case WarpEntry::WALK_NORTH:
        return Direction::NORTH;
    case WarpEntry::WALK_SOUTH:
        return Direction::SOUTH;
    case WarpEntry::WALK_WEST:
        return Direction::WEST;
    case WarpEntry::WALK_EAST:
        return Direction::EAST;
    default:
        return std::nullopt;
    }
}
bool walks_out_on_arrival(const MapData& map, const TilePos& point){
    return is_door(map, point) || warp_entry(map, point) == WarpEntry::CAVE_ENTRANCE;
}

void hide_objects(MapData& map, const std::set<std::string>& flags){
    std::vector<MapObject> shown;
    for (const MapObject& object : map.objects){
        if (!object.hide_flag || flags.count(*object.hide_flag) == 0){
            shown.push_back(object);
        }
    }
    map.objects = std::move(shown);
}

const MapObject* object_at(const MapData& map, const TilePos& point){
    for (const MapObject& object : map.objects){
        const Pokemon::TileBox box = object.wander.value_or(
            Pokemon::TileBox{.x0 = object.x, .y0 = object.y, .x1 = object.x, .y1 = object.y}
        );
        if (box.x0 <= point.x && point.x <= box.x1 && box.y0 <= point.y && point.y <= box.y1){
            return &object;
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
