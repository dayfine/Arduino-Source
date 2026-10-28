/*  Tile Grid
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <tuple>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* direction_name(GridDirection direction){
    switch (direction){
    case GridDirection::NORTH:  return "north";
    case GridDirection::EAST:   return "east";
    case GridDirection::SOUTH:  return "south";
    case GridDirection::WEST:   return "west";
    }
    return "?";
}

TilePos TilePos::next(GridDirection direction) const{
    switch (direction){
    case GridDirection::NORTH:  return {x, y - 1};
    case GridDirection::EAST:   return {x + 1, y};
    case GridDirection::SOUTH:  return {x, y + 1};
    case GridDirection::WEST:   return {x - 1, y};
    }
    return *this;
}

char tile_char(TileType type){
    switch (type){
    case TileType::BLOCKED:     return '#';
    case TileType::WALKABLE:    return '.';
    case TileType::TALL_GRASS:  return '"';
    case TileType::LEDGE:       return 'v';
    case TileType::NPC:         return 'N';
    case TileType::SIGN:        return 'S';
    case TileType::DOOR:        return 'D';
    case TileType::UNKNOWN:     return '?';
    }
    return '?';
}

std::string to_string(const TilePos& pos){
    return "(" + std::to_string(pos.x) + "," + std::to_string(pos.y) + ")";
}



TileGrid::TileGrid(const std::vector<std::string>& rows)
    : m_width(rows.empty() ? 0 : (int)rows[0].size())
    , m_height((int)rows.size())
{
    m_tiles.reserve((size_t)m_width * m_height);
    for (size_t y = 0; y < rows.size(); y++){
        const std::string& row = rows[y];
        if ((int)row.size() != m_width){
            throw InternalProgramError(
                nullptr, PA_CURRENT_FUNCTION,
                "Tile grid row " + std::to_string(y) + " has " + std::to_string(row.size()) +
                " tiles, expected " + std::to_string(m_width) + "."
            );
        }
        for (size_t x = 0; x < row.size(); x++){
            switch (row[x]){
            case '#':   m_tiles.push_back(TileType::BLOCKED);       break;
            case '.':   m_tiles.push_back(TileType::WALKABLE);      break;
            case '"':   m_tiles.push_back(TileType::TALL_GRASS);    break;
            case 'v':   m_tiles.push_back(TileType::LEDGE);         break;
            case 'N':   m_tiles.push_back(TileType::NPC);           break;
            case 'S':   m_tiles.push_back(TileType::SIGN);          break;
            case 'D':   m_tiles.push_back(TileType::DOOR);          break;
            case '?':   m_tiles.push_back(TileType::UNKNOWN);       break;
            default:
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    std::string("Unknown tile '") + row[x] + "' at " +
                    to_string(TilePos{(int)x, (int)y}) + "."
                );
            }
        }
    }
}

std::vector<std::string> TileGrid::rows() const{
    std::vector<std::string> rows;
    for (int y = 0; y < m_height; y++){
        std::string row;
        for (int x = 0; x < m_width; x++){
            row += tile_char(at({x, y}));
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

bool TileGrid::in_bounds(const TilePos& pos) const{
    return 0 <= pos.x && pos.x < m_width && 0 <= pos.y && pos.y < m_height;
}

TileType TileGrid::at(const TilePos& pos) const{
    if (!in_bounds(pos)){
        return TileType::UNKNOWN;
    }
    return m_tiles[(size_t)pos.y * m_width + pos.x];
}

bool TileGrid::standable(const TilePos& pos) const{
    const TileType type = at(pos);
    return type == TileType::WALKABLE || type == TileType::TALL_GRASS;
}

std::optional<TilePos> TileGrid::step(const TilePos& from, GridDirection direction) const{
    const TilePos next = from.next(direction);
    if (standable(next)){
        return next;
    }
    if (at(next) == TileType::LEDGE && direction == GridDirection::SOUTH){
        const TilePos landing = next.next(direction);
        if (standable(landing)){
            return landing;
        }
    }
    return std::nullopt;
}

bool TileGrid::at_wall(const TilePos& pos, GridDirection direction) const{
    const TileType next = at(pos.next(direction));
    return next != TileType::DOOR && next != TileType::UNKNOWN && !step(pos, direction).has_value();
}



WallHit walk_to_wall(const TileGrid& grid, const TilePos& from, GridDirection direction){
    WallHit hit;
    hit.stop = from;
    while (true){
        const TilePos before = hit.stop;
        const std::optional<TilePos> next = grid.step(before, direction);
        if (!next){
            hit.wall = before.next(direction);
            hit.anchored = grid.at_wall(before, direction);
            return hit;
        }
        hit.tiles += std::abs(next->x - before.x) + std::abs(next->y - before.y);
        hit.stop = *next;
    }
}



std::string to_string(const TileLeg& leg){
    return std::string(direction_name(leg.direction)) + " " + std::to_string(leg.tiles) +
        (leg.ends_at_wall ? " (to wall)" : " (open)");
}


namespace{

//  Search state: a tile and the direction of the leg in progress.
//  Direction index 4 means no leg has started yet.
constexpr int NO_LEG = 4;
constexpr GridDirection DIRECTIONS[] = {
    GridDirection::NORTH, GridDirection::EAST, GridDirection::SOUTH, GridDirection::WEST,
};

}

std::optional<std::vector<TileLeg>> plan_tile_route(
    const TileGrid& grid,
    const TilePos& from, const TilePos& to,
    const TilePlanCosts& costs
){
    if (!grid.standable(from) || !grid.standable(to)){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION,
            "Route ends must be standable: " + to_string(from) + " -> " + to_string(to) + "."
        );
    }
    if (from == to){
        return std::vector<TileLeg>();
    }

    const int width = grid.width();
    const size_t states = (size_t)width * grid.height() * 5;
    auto index = [&](const TilePos& pos, int heading){
        return ((size_t)pos.y * width + pos.x) * 5 + heading;
    };
    constexpr long long INF = std::numeric_limits<long long>::max();
    std::vector<long long> best(states, INF);
    std::vector<size_t> parent(states, states);

    //  Closing a leg costs extra when it stops in the open.
    auto leg_end_cost = [&](const TilePos& pos, int heading) -> long long{
        if (heading == NO_LEG || grid.at_wall(pos, DIRECTIONS[heading])){
            return 0;
        }
        return costs.leg_not_at_wall;
    };

    using Entry = std::tuple<long long, size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    const size_t start = index(from, NO_LEG);
    best[start] = 0;
    queue.emplace(0, start);

    size_t goal = states;
    long long goal_cost = INF;
    while (!queue.empty()){
        const auto [cost, state] = queue.top();
        queue.pop();
        if (cost != best[state]){
            continue;
        }
        if (cost >= goal_cost){
            break;
        }
        const size_t tile = state / 5;
        const int heading = (int)(state % 5);
        const TilePos pos{(int)(tile % width), (int)(tile / width)};

        if (pos == to){
            const long long total = cost + leg_end_cost(pos, heading);
            if (total < goal_cost){
                goal_cost = total;
                goal = state;
            }
            continue;
        }

        for (int d = 0; d < 4; d++){
            const std::optional<TilePos> next = grid.step(pos, DIRECTIONS[d]);
            if (!next){
                continue;
            }
            const int tiles = std::abs(next->x - pos.x) + std::abs(next->y - pos.y);
            long long step_cost = (long long)costs.per_tile * tiles;
            if (grid.at(*next) == TileType::TALL_GRASS){
                step_cost += costs.per_grass_tile;
            }
            if (d != heading){
                step_cost += costs.per_leg + leg_end_cost(pos, heading);
            }
            const size_t next_state = index(*next, d);
            if (cost + step_cost < best[next_state]){
                best[next_state] = cost + step_cost;
                parent[next_state] = state;
                queue.emplace(best[next_state], next_state);
            }
        }
    }
    if (goal == states){
        return std::nullopt;
    }

    //  Walk back to the start, then merge steps into legs.
    std::vector<size_t> path;
    for (size_t state = goal; state != start; state = parent[state]){
        path.push_back(state);
    }
    std::reverse(path.begin(), path.end());

    std::vector<TileLeg> legs;
    TilePos pos = from;
    for (size_t state : path){
        const size_t tile = state / 5;
        const TilePos next{(int)(tile % width), (int)(tile / width)};
        const GridDirection direction = DIRECTIONS[state % 5];
        const size_t tiles = std::abs(next.x - pos.x) + std::abs(next.y - pos.y);
        if (legs.empty() || legs.back().direction != direction){
            if (!legs.empty()){
                legs.back().ends_at_wall = grid.at_wall(pos, legs.back().direction);
            }
            legs.push_back(TileLeg{direction, 0, false});
        }
        legs.back().tiles += tiles;
        pos = next;
    }
    legs.back().ends_at_wall = grid.at_wall(pos, legs.back().direction);
    return legs;
}

std::vector<TilePos> sight_tiles(const TileGrid& grid, const TrainerSight& trainer){
    std::vector<TilePos> tiles;
    TilePos pos = trainer.pos;
    for (int i = 0; i < trainer.range; i++){
        pos = pos.next(trainer.facing);
        if (!grid.standable(pos)){
            break;
        }
        tiles.push_back(pos);
    }
    return tiles;
}

std::vector<std::vector<std::string>> legs_crossing_sight(
    const TileGrid& grid,
    const TilePos& from,
    const std::vector<TileLeg>& legs,
    const std::vector<TrainerSight>& trainers
){
    std::vector<std::vector<TilePos>> sights;
    for (const TrainerSight& trainer : trainers){
        sights.push_back(sight_tiles(grid, trainer));
    }
    std::vector<std::vector<std::string>> crossings;
    TilePos pos = from;
    for (const TileLeg& leg : legs){
        const std::vector<TilePos> tiles = trace_legs(grid, pos, {leg});
        std::vector<std::string> seen_by;
        for (size_t t = 0; t < trainers.size(); t++){
            for (size_t i = 1; i < tiles.size(); i++){
                if (std::find(sights[t].begin(), sights[t].end(), tiles[i]) != sights[t].end()){
                    seen_by.push_back(trainers[t].name);
                    break;
                }
            }
        }
        crossings.push_back(std::move(seen_by));
        pos = tiles.back();
    }
    return crossings;
}

std::vector<TilePos> trace_legs(
    const TileGrid& grid,
    const TilePos& from,
    const std::vector<TileLeg>& legs
){
    std::vector<TilePos> tiles{from};
    for (const TileLeg& leg : legs){
        size_t travelled = 0;
        while (travelled < leg.tiles){
            const TilePos pos = tiles.back();
            const std::optional<TilePos> next = grid.step(pos, leg.direction);
            if (!next){
                throw InternalProgramError(
                    nullptr, PA_CURRENT_FUNCTION,
                    "Leg " + to_string(leg) + " is stopped at " + to_string(pos) + "."
                );
            }
            travelled += std::abs(next->x - pos.x) + std::abs(next->y - pos.y);
            tiles.push_back(*next);
        }
        if (travelled != leg.tiles){
            throw InternalProgramError(
                nullptr, PA_CURRENT_FUNCTION,
                "Leg " + to_string(leg) + " overshoots on a ledge hop."
            );
        }
    }
    return tiles;
}



}
}
