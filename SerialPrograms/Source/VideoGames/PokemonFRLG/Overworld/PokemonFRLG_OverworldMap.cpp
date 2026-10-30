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

//  Appends row `y` of `text` to `map`.
void add_row(OverworldMap& map, const MapText& text, int y){
    const std::string& terrain = text.terrain[y];
    const std::string& elevation = text.elevation[y];
    if ((int)terrain.size() != map.width || (int)elevation.size() != map.width){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, map.name + ": row " + std::to_string(y) + " has the wrong length.");
    }
    for (int x = 0; x < map.width; x++){
        const char tile = terrain[x];
        if (!known_terrain(tile)){
            throw InternalProgramError(
                nullptr, PA_CURRENT_FUNCTION,
                map.name + ": unknown terrain '" + std::string(1, tile) + "' at " + to_string(TilePos{x, y})
            );
        }
        const std::optional<uint8_t> level = elevation_level(elevation[x]);
        if (!level){
            throw InternalProgramError(
                nullptr, PA_CURRENT_FUNCTION,
                map.name + ": bad elevation '" + std::string(1, elevation[x]) + "' at " + to_string(TilePos{x, y})
            );
        }
        map.terrain.push_back(tile);
        map.elevation.push_back(*level);
    }
}

}


OverworldMap make_overworld_map(const MapText& text){
    OverworldMap map;
    map.name = text.name;
    map.height = (int)text.terrain.size();
    map.warps = text.warps;
    map.objects = text.objects;
    map.connections = text.connections;
    if (map.height == 0 || text.elevation.size() != text.terrain.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, map.name + ": terrain and elevation need the same number of rows.");
    }
    map.width = (int)text.terrain[0].size();
    for (int y = 0; y < map.height; y++){
        add_row(map, text, y);
    }
    return map;
}

bool in_bounds(const OverworldMap& map, const TilePos& point){
    return 0 <= point.x && point.x < map.width && 0 <= point.y && point.y < map.height;
}
char terrain_at(const OverworldMap& map, const TilePos& point){
    if (!in_bounds(map, point)){
        return '#';
    }
    return map.terrain[(size_t)point.y * map.width + point.x];
}
uint8_t elevation_at(const OverworldMap& map, const TilePos& point){
    if (!in_bounds(map, point)){
        return ELEVATION_IGNORED;
    }
    return map.elevation[(size_t)point.y * map.width + point.x];
}

bool is_wall(const OverworldMap& map, const TilePos& point){
    const char tile = terrain_at(map, point);
    return tile == '#' || tile == 'D' || tile == 'J';
}
bool is_ledge(const OverworldMap& map, const TilePos& point){
    return terrain_at(map, point) == 'J';
}
bool is_water(const OverworldMap& map, const TilePos& point){
    return terrain_at(map, point) == '~';
}
bool is_tall_grass(const OverworldMap& map, const TilePos& point){
    return terrain_at(map, point) == '"';
}

bool blocks_entering(const OverworldMap& map, const TilePos& point, Direction direction){
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
bool blocks_leaving(const OverworldMap& map, const TilePos& point, Direction direction){
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

bool warps_on_step(const OverworldMap& map, const TilePos& point){
    const char tile = terrain_at(map, point);
    return tile == 'C' || tile == 'L';
}
bool is_door(const OverworldMap& map, const TilePos& point){
    return terrain_at(map, point) == 'D';
}
std::optional<Direction> exit_mat(const OverworldMap& map, const TilePos& point){
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
bool walks_out_on_arrival(const OverworldMap& map, const TilePos& point){
    const char tile = terrain_at(map, point);
    return tile == 'D' || tile == 'C';
}

void hide_objects(OverworldMap& map, const std::set<std::string>& flags){
    std::vector<MapObject> shown;
    for (const MapObject& object : map.objects){
        if (object.flag.empty() || flags.count(object.flag) == 0){
            shown.push_back(object);
        }
    }
    map.objects = std::move(shown);
}

const MapObject* object_at(const OverworldMap& map, const TilePos& point){
    for (const MapObject& object : map.objects){
        if (std::abs(object.x - point.x) <= object.wander_x && std::abs(object.y - point.y) <= object.wander_y){
            return &object;
        }
    }
    return nullptr;
}
const MapWarp* warp_at(const OverworldMap& map, const TilePos& point){
    for (const MapWarp& warp : map.warps){
        if (warp.x == point.x && warp.y == point.y){
            return &warp;
        }
    }
    return nullptr;
}



StepKind classify_step(const OverworldMap& map, const PlayerState& state, Direction direction){
    return classify_step_onto(map, state, direction, map, state.position.next(direction));
}

StepKind classify_step_onto(
    const OverworldMap& from, const PlayerState& state, Direction direction,
    const OverworldMap& to, const TilePos& destination
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

PlayerState after_step(const OverworldMap& map, const PlayerState& state, Direction direction, StepKind kind){
    return after_step_onto(map, state, map, state.position.next(direction), kind);
}

PlayerState after_step_onto(
    const OverworldMap& from, const PlayerState& state,
    const OverworldMap& to, const TilePos& destination, StepKind kind
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
