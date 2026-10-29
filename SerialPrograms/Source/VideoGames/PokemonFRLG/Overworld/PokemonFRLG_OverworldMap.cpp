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


//  Elevation rules of the game (IsElevationMismatchAt() and
//  ObjectEventUpdateElevation()).
constexpr uint8_t ELEVATION_ANY = 0;        //  joins elevations
constexpr uint8_t ELEVATION_IGNORED = 15;   //  doesn't take part
constexpr uint8_t ELEVATION_GROUND = 3;     //  where Surf starts and ends


const char* direction_name(MapDirection direction){
    switch (direction){
    case MapDirection::NORTH:
        return "north";
    case MapDirection::EAST:
        return "east";
    case MapDirection::SOUTH:
        return "south";
    case MapDirection::WEST:
        return "west";
    }
    return "?";
}

MapPoint MapPoint::next(MapDirection direction) const{
    switch (direction){
    case MapDirection::NORTH:
        return {x, y - 1};
    case MapDirection::EAST:
        return {x + 1, y};
    case MapDirection::SOUTH:
        return {x, y + 1};
    case MapDirection::WEST:
        return {x - 1, y};
    }
    return *this;
}

std::string to_string(const MapPoint& point){
    return "(" + std::to_string(point.x) + ", " + std::to_string(point.y) + ")";
}



OverworldMap::OverworldMap(const MapText& text)
    : m_name(text.name)
    , m_height((int)text.terrain.size())
    , m_warps(text.warps)
    , m_objects(text.objects)
{
    if (m_height == 0 || text.elevation.size() != text.terrain.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, m_name + ": terrain and elevation need the same number of rows.");
    }
    m_width = (int)text.terrain[0].size();
    for (int y = 0; y < m_height; y++){
        const std::string& terrain = text.terrain[y];
        const std::string& elevation = text.elevation[y];
        if ((int)terrain.size() != m_width || (int)elevation.size() != m_width){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, m_name + ": row " + std::to_string(y) + " has the wrong length.");
        }
        for (int x = 0; x < m_width; x++){
            const char tile = terrain[x];
            switch (tile){
            case '#':
            case '.':
            case '~':
            case 'n':
            case 's':
            case 'e':
            case 'w':
                break;
            default:
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    m_name + ": unknown terrain '" + std::string(1, tile) + "' at " + to_string(MapPoint{x, y})
                );
            }
            const char digit = elevation[x];
            int level;
            if ('0' <= digit && digit <= '9'){
                level = digit - '0';
            }else if ('a' <= digit && digit <= 'f'){
                level = digit - 'a' + 10;
            }else{
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    m_name + ": bad elevation '" + std::string(1, digit) + "' at " + to_string(MapPoint{x, y})
                );
            }
            m_terrain.push_back(tile);
            m_elevation.push_back((uint8_t)level);
        }
    }
}

bool OverworldMap::in_bounds(const MapPoint& point) const{
    return 0 <= point.x && point.x < m_width && 0 <= point.y && point.y < m_height;
}
char OverworldMap::terrain(const MapPoint& point) const{
    if (!in_bounds(point)){
        return '#';
    }
    return m_terrain[(size_t)point.y * m_width + point.x];
}
bool OverworldMap::wall(const MapPoint& point) const{
    return terrain(point) == '#';
}
bool OverworldMap::water(const MapPoint& point) const{
    return terrain(point) == '~';
}
uint8_t OverworldMap::elevation(const MapPoint& point) const{
    if (!in_bounds(point)){
        return ELEVATION_IGNORED;
    }
    return m_elevation[(size_t)point.y * m_width + point.x];
}

bool OverworldMap::blocks_entering(const MapPoint& point, MapDirection direction) const{
    const char tile = terrain(point);
    switch (direction){
    case MapDirection::SOUTH:
        return tile == 'n';
    case MapDirection::NORTH:
        return tile == 's';
    case MapDirection::WEST:
        return tile == 'e';
    case MapDirection::EAST:
        return tile == 'w';
    }
    return false;
}
bool OverworldMap::blocks_leaving(const MapPoint& point, MapDirection direction) const{
    const char tile = terrain(point);
    switch (direction){
    case MapDirection::NORTH:
        return tile == 'n';
    case MapDirection::SOUTH:
        return tile == 's';
    case MapDirection::EAST:
        return tile == 'e';
    case MapDirection::WEST:
        return tile == 'w';
    }
    return false;
}

const MapObject* OverworldMap::object_at(const MapPoint& point) const{
    for (const MapObject& object : m_objects){
        if (object.x == point.x && object.y == point.y){
            return &object;
        }
    }
    return nullptr;
}
const MapWarp* OverworldMap::warp_at(const MapPoint& point) const{
    for (const MapWarp& warp : m_warps){
        if (warp.x == point.x && warp.y == point.y){
            return &warp;
        }
    }
    return nullptr;
}



StepKind classify_step(const OverworldMap& map, const PlayerState& state, MapDirection direction){
    const MapPoint destination = state.position.next(direction);
    if (map.wall(destination)
        || map.blocks_leaving(state.position, direction)
        || map.blocks_entering(destination, direction)
    ){
        return StepKind::BLOCKED;
    }

    const uint8_t level = map.elevation(destination);
    const bool mismatch = state.elevation != ELEVATION_ANY
        && level != ELEVATION_ANY && level != ELEVATION_IGNORED
        && level != state.elevation;
    if (mismatch){
        //  CanStopSurfing() and IsPlayerFacingSurfableFishableWater().
        if (state.surfing && level == ELEVATION_GROUND && map.object_at(destination) == nullptr){
            return StepKind::STOP_SURFING;
        }
        if (!state.surfing && state.elevation == ELEVATION_GROUND && map.water(destination)){
            return StepKind::START_SURF;
        }
        return StepKind::BLOCKED;
    }

    if (map.object_at(destination) != nullptr){
        return StepKind::BLOCKED;
    }
    return StepKind::WALK;
}

PlayerState after_step(const OverworldMap& map, const PlayerState& state, MapDirection direction, StepKind kind){
    PlayerState next = state;
    next.position = state.position.next(direction);
    const uint8_t level = map.elevation(next.position);
    if (level != ELEVATION_IGNORED && map.elevation(state.position) != ELEVATION_IGNORED){
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
