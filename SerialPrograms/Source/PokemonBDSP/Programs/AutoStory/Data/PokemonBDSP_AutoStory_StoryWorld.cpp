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
    check_overlap(placed.name, placed);
    m_maps.push_back(std::move(placed));
    rebuild();
}

//  Map images show a border of trees past their edges, so two maps may
//  share walls with nobody on them, but nothing else.
void StoryWorld::check_overlap(const std::string& name, const Placed& map) const{
    const auto free_wall = [](const Placed& placed, const TilePos& pos){
        return placed.tiles.at(pos) == TileKind::WALL &&
            std::find(placed.occupied.begin(), placed.occupied.end(), pos) == placed.occupied.end();
    };
    for (const Placed& placed : m_maps){
        for (int y = 0; y < map.tiles.height; y++){
            for (int x = 0; x < map.tiles.width; x++){
                const TilePos other{map.offset.x + x - placed.offset.x, map.offset.y + y - placed.offset.y};
                if (placed.tiles.contains(other) && (!free_wall(map, {x, y}) || !free_wall(placed, other))){
                    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
                        "Map {} overlaps {} at its tile {}.", name, placed.name, Pokemon::to_string(TilePos{x, y})
                    ));
                }
            }
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
        const TilePos corner{iter->offset.x - low.x, iter->offset.y - low.y};
        for (int y = 0; y < iter->tiles.height; y++){
            for (int x = 0; x < iter->tiles.width; x++){
                m_cells.at({corner.x + x, corner.y + y}) = Cell{true, iter->tiles.at({x, y}), false};
            }
        }
        for (const TilePos& pos : iter->occupied){
            m_cells.at({corner.x + pos.x, corner.y + pos.y}).occupied = true;
        }
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
    return &m_cells.at(pos);
}


bool StoryWorld::covered(const TilePos& pos) const{
    return cell(pos) != nullptr;
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

bool StoryWorld::standable(const TilePos& pos) const{
    const Cell* found = cell(pos);
    return found != nullptr && !found->occupied &&
        (found->tile == TileKind::OPEN || found->tile == TileKind::TALL_GRASS);
}

std::optional<TilePos> StoryWorld::step(const TilePos& from, Direction direction) const{
    const TilePos next = from.next(direction);
    if (standable(next)){
        return next;
    }
    const Cell* found = cell(next);
    if (found == nullptr || found->occupied || ledge_direction(found->tile) != direction){
        return std::nullopt;
    }
    const TilePos landing = next.next(direction);
    if (standable(landing)){
        return landing;
    }
    return std::nullopt;
}

bool StoryWorld::at_wall(const TilePos& pos, Direction direction) const{
    const std::optional<TileKind> next = tile(pos.next(direction));
    return next && *next != TileKind::DOOR && !step(pos, direction).has_value();
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

std::string StoryWorld::describe(const TilePos& world) const{
    const std::optional<MapTile> local = to_local(world);
    if (!local){
        return "world " + Pokemon::to_string(world);
    }
    return local->map + " " + Pokemon::to_string(local->pos);
}



}
}
}
