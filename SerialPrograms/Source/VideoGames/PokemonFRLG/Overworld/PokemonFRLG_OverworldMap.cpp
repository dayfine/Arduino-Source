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


//  Elevation rules of the game (IsElevationMismatchAt() and
//  ObjectEventUpdateElevation()).
constexpr uint8_t ELEVATION_ANY = 0;        //  joins elevations
constexpr uint8_t ELEVATION_IGNORED = 15;   //  doesn't take part
constexpr uint8_t ELEVATION_GROUND = 3;     //  where Surf starts and ends


namespace{

bool known_terrain(char tile){
    switch (tile){
    case '#':
    case 'J':
    case '.':
    case '"':
    case '~':
    case 'n':
    case 's':
    case 'e':
    case 'w':
    case 'D':
    case 'C':
    case 'L':
    case '^':
    case 'v':
    case '<':
    case '>':
        return true;
    default:
        return false;
    }
}

//  The level of an elevation digit, or nothing if it isn't one.
std::optional<uint8_t> elevation_level(char digit){
    if ('0' <= digit && digit <= '9'){
        return (uint8_t)(digit - '0');
    }
    if ('a' <= digit && digit <= 'f'){
        return (uint8_t)(digit - 'a' + 10);
    }
    return std::nullopt;
}

[[noreturn]] void throw_bad_map(const MapData& map, const std::string& message){
    throw ParseException(map.name + ": " + message);
}

void check_row(const MapData& map, int y){
    const std::string& tiles = map.tiles[y];
    const std::string& elevation = map.elevation[y];
    if ((int)tiles.size() != map.width || (int)elevation.size() != map.width){
        throw_bad_map(map, "row " + std::to_string(y) + " of tiles or elevation isn't " + std::to_string(map.width) + " wide.");
    }
    for (int x = 0; x < map.width; x++){
        if (!known_terrain(tiles[x])){
            throw_bad_map(map, "unknown tile '" + std::string(1, tiles[x]) + "' at " + to_string(TilePos{x, y}) + ".");
        }
        if (!elevation_level(elevation[x])){
            throw_bad_map(map, "bad elevation '" + std::string(1, elevation[x]) + "' at " + to_string(TilePos{x, y}) + ".");
        }
    }
}

}


void check_map(const MapData& map){
    if (map.width <= 0 || map.height <= 0){
        throw_bad_map(map, "width and height must be positive.");
    }
    if ((int)map.tiles.size() != map.height || (int)map.elevation.size() != map.height){
        throw_bad_map(map, "tiles and elevation need " + std::to_string(map.height) + " rows.");
    }
    for (int y = 0; y < map.height; y++){
        check_row(map, y);
    }
}

bool in_bounds(const MapData& map, const TilePos& point){
    return 0 <= point.x && point.x < map.width && 0 <= point.y && point.y < map.height;
}
char terrain_at(const MapData& map, const TilePos& point){
    if (!in_bounds(map, point)){
        return '#';
    }
    return map.tiles[point.y][point.x];
}
uint8_t elevation_at(const MapData& map, const TilePos& point){
    if (!in_bounds(map, point)){
        return ELEVATION_IGNORED;
    }
    return elevation_level(map.elevation[point.y][point.x]).value_or(ELEVATION_IGNORED);
}

bool is_wall(const MapData& map, const TilePos& point){
    const char tile = terrain_at(map, point);
    return tile == '#' || tile == 'D' || tile == 'J';
}
bool is_ledge(const MapData& map, const TilePos& point){
    return terrain_at(map, point) == 'J';
}
bool is_water(const MapData& map, const TilePos& point){
    return terrain_at(map, point) == '~';
}
bool is_tall_grass(const MapData& map, const TilePos& point){
    return terrain_at(map, point) == '"';
}

bool blocks_entering(const MapData& map, const TilePos& point, Direction direction){
    const char tile = terrain_at(map, point);
    switch (direction){
    case Direction::SOUTH:
        return tile == 'n';
    case Direction::NORTH:
        return tile == 's';
    case Direction::WEST:
        return tile == 'e';
    case Direction::EAST:
        return tile == 'w';
    }
    return false;
}
bool blocks_leaving(const MapData& map, const TilePos& point, Direction direction){
    const char tile = terrain_at(map, point);
    switch (direction){
    case Direction::NORTH:
        return tile == 'n';
    case Direction::SOUTH:
        return tile == 's';
    case Direction::EAST:
        return tile == 'e';
    case Direction::WEST:
        return tile == 'w';
    }
    return false;
}

bool warps_on_step(const MapData& map, const TilePos& point){
    const char tile = terrain_at(map, point);
    return tile == 'C' || tile == 'L';
}
bool is_door(const MapData& map, const TilePos& point){
    return terrain_at(map, point) == 'D';
}
std::optional<Direction> exit_mat(const MapData& map, const TilePos& point){
    switch (terrain_at(map, point)){
    case '^':
        return Direction::NORTH;
    case 'v':
        return Direction::SOUTH;
    case '<':
        return Direction::WEST;
    case '>':
        return Direction::EAST;
    default:
        return std::nullopt;
    }
}
bool walks_out_on_arrival(const MapData& map, const TilePos& point){
    const char tile = terrain_at(map, point);
    return tile == 'D' || tile == 'C';
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
