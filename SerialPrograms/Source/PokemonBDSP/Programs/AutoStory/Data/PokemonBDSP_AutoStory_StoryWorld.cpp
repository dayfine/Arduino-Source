/*  BDSP AutoStory - Story world
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::TileKind;
using Pokemon::TilePos;


namespace{

//  The way a ledge is hopped, or nothing for any other tile.
std::optional<Direction> ledge_direction(TileKind tile){
    switch (tile){
    case TileKind::LEDGE_NORTH:
        return Direction::NORTH;
    case TileKind::LEDGE_SOUTH:
        return Direction::SOUTH;
    case TileKind::LEDGE_WEST:
        return Direction::WEST;
    case TileKind::LEDGE_EAST:
        return Direction::EAST;
    case TileKind::WALL:
    case TileKind::OPEN:
    case TileKind::TALL_GRASS:
    case TileKind::WATER:
    case TileKind::DOOR:
    case TileKind::SIGN:
        return std::nullopt;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown TileKind.");
}

}


void StoryWorld::add(std::string name, const Pokemon::MapData& map, const TilePos& offset){
    for (const Placed& placed : m_maps){
        if (placed.name == name){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Map placed twice: " + name);
        }
    }
    Placed placed{std::move(name), map.tiles, {}, offset};
    for (const Pokemon::MapObject& object : map.objects){
        if (!object.wander){
            placed.occupied.push_back({object.x, object.y});
        }
    }
    check_overlap(placed);
    m_maps.push_back(std::move(placed));
    rebuild();
}

//  Map images show a border of trees past their edges, so two maps may
//  share walls with nobody on them, but nothing else.
std::optional<TilePos> StoryWorld::conflict(const Placed& map, const Placed& placed){
    const auto free_wall = [](const Placed& in, const TilePos& pos){
        return in.tiles.at(pos) == TileKind::WALL &&
            std::find(in.occupied.begin(), in.occupied.end(), pos) == in.occupied.end();
    };
    for (int y = 0; y < map.tiles.height; y++){
        for (int x = 0; x < map.tiles.width; x++){
            const TilePos other{map.offset.x + x - placed.offset.x, map.offset.y + y - placed.offset.y};
            if (placed.tiles.contains(other) && (!free_wall(map, {x, y}) || !free_wall(placed, other))){
                return TilePos{x, y};
            }
        }
    }
    return std::nullopt;
}

void StoryWorld::check_overlap(const Placed& map) const{
    for (const Placed& placed : m_maps){
        if (const std::optional<TilePos> at = conflict(map, placed)){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
                "Map {} overlaps {} at its tile {}.", map.name, placed.name, Pokemon::to_string(*at)
            ));
        }
    }
}

void StoryWorld::rebuild(){
    TilePos low = m_maps[0].offset;
    TilePos high = low;
    for (const Placed& placed : m_maps){
        low.x = std::min(low.x, placed.offset.x);
        low.y = std::min(low.y, placed.offset.y);
        high.x = std::max(high.x, placed.offset.x + placed.tiles.width);
        high.y = std::max(high.y, placed.offset.y + placed.tiles.height);
    }
    //  Block and uncover lists are in world tiles of the old origin.
    const TilePos shift{m_origin.x - low.x, m_origin.y - low.y};
    for (std::vector<TilePos>* list : {&m_blocked, &m_uncovered}){
        for (TilePos& pos : *list){
            pos = {pos.x + shift.x, pos.y + shift.y};
        }
    }
    m_origin = low;
    m_cells = Pokemon::Grid<Cell>{high.x - low.x, high.y - low.y, {}};
    m_cells.cells.resize((size_t)m_cells.width * m_cells.height);
    for (auto iter = m_maps.rbegin(); iter != m_maps.rend(); ++iter){
        paint(*iter, {iter->offset.x - low.x, iter->offset.y - low.y});
    }
    for (const TilePos& pos : m_blocked){
        if (Cell* blocked = cell(pos)){
            blocked->occupied = true;
        }
    }
    for (const TilePos& pos : m_uncovered){
        if (Cell* uncovered = cell(pos)){
            uncovered->covered = false;
        }
    }
}

void StoryWorld::paint(const Placed& placed, const TilePos& corner){
    for (int y = 0; y < placed.tiles.height; y++){
        for (int x = 0; x < placed.tiles.width; x++){
            Pokemon::cell_at(m_cells, {corner.x + x, corner.y + y}) = Cell{true, placed.tiles.at({x, y}), false};
        }
    }
    for (const TilePos& pos : placed.occupied){
        Pokemon::cell_at(m_cells, {corner.x + pos.x, corner.y + pos.y}).occupied = true;
    }
}

const StoryWorld::Cell* StoryWorld::cell(const TilePos& pos) const{
    if (!m_cells.contains(pos) || !m_cells.at(pos).covered){
        return nullptr;
    }
    return &m_cells.at(pos);
}
StoryWorld::Cell* StoryWorld::cell(const TilePos& pos){
    if (!m_cells.contains(pos) || !m_cells.at(pos).covered){
        return nullptr;
    }
    return &Pokemon::cell_at(m_cells, pos);
}


std::optional<TileKind> StoryWorld::tile(const TilePos& pos) const{
    const Cell* found = cell(pos);
    if (found == nullptr){
        return std::nullopt;
    }
    return found->tile;
}

bool StoryWorld::occupied(const TilePos& pos) const{
    const Cell* found = cell(pos);
    return found != nullptr && found->occupied;
}

void StoryWorld::block(const TilePos& pos){
    if (Cell* found = cell(pos)){
        found->occupied = true;
        m_blocked.push_back(pos);
    }
}

void StoryWorld::uncover(const TilePos& pos){
    if (Cell* found = cell(pos)){
        found->covered = false;
        m_uncovered.push_back(pos);
    }
}


TilePos StoryWorld::to_world(const std::string& map, const TilePos& local) const{
    for (const Placed& placed : m_maps){
        if (placed.name == map){
            return {placed.offset.x - m_origin.x + local.x, placed.offset.y - m_origin.y + local.y};
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown map: " + map);
}

std::optional<MapTile> StoryWorld::to_local(const TilePos& world) const{
    for (const Placed& placed : m_maps){
        const TilePos local{world.x + m_origin.x - placed.offset.x, world.y + m_origin.y - placed.offset.y};
        if (placed.tiles.contains(local)){
            return MapTile{placed.name, local};
        }
    }
    return std::nullopt;
}



bool covered(const StoryWorld& world, const TilePos& pos){
    return world.tile(pos).has_value();
}

bool standable(const StoryWorld& world, const TilePos& pos){
    const std::optional<TileKind> tile = world.tile(pos);
    return tile && (*tile == TileKind::OPEN || *tile == TileKind::TALL_GRASS) && !world.occupied(pos);
}

std::optional<TilePos> step(const StoryWorld& world, const TilePos& from, Direction direction){
    const TilePos next = from.next(direction);
    if (standable(world, next)){
        return next;
    }
    const std::optional<TileKind> tile = world.tile(next);
    if (!tile || world.occupied(next) || ledge_direction(*tile) != direction){
        return std::nullopt;
    }
    const TilePos landing = next.next(direction);
    if (standable(world, landing)){
        return landing;
    }
    return std::nullopt;
}

bool at_wall(const StoryWorld& world, const TilePos& pos, Direction direction){
    const std::optional<TileKind> next = world.tile(pos.next(direction));
    return next && *next != TileKind::DOOR && !step(world, pos, direction).has_value();
}

std::string describe(const StoryWorld& world, const TilePos& pos){
    const std::optional<MapTile> local = world.to_local(pos);
    if (!local){
        return "world " + Pokemon::to_string(pos);
    }
    return local->map + " " + Pokemon::to_string(local->pos);
}



}
}
}
