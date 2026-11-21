/*  BDSP AutoStory PR #1717 Map & World Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Loads the 22 verified Sinnoh runtime maps (PlayerHouse2F through
 *  FloaromaTown) plus small synthetic interiors (TrainersSchool,
 *  PokemonCenter, OreburghMart) for offline simulation of PR #1717.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_Map_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_Map_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

enum class SimDir{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3,
};

inline const char* sim_dir_name(SimDir d){
    switch (d){
    case SimDir::NORTH: return "north";
    case SimDir::EAST:  return "east";
    case SimDir::SOUTH: return "south";
    case SimDir::WEST:  return "west";
    }
    return "?";
}

struct SimTile{
    int x = 0;
    int y = 0;
    bool operator==(const SimTile& o) const = default;
    bool operator<(const SimTile& o) const{
        return x != o.x ? x < o.x : y < o.y;
    }
    SimTile next(SimDir d) const{
        switch (d){
        case SimDir::NORTH: return {x, y - 1};
        case SimDir::EAST:  return {x + 1, y};
        case SimDir::SOUTH: return {x, y + 1};
        case SimDir::WEST:  return {x - 1, y};
        }
        return *this;
    }
};

enum class SimTileKind{
    WALL,
    OPEN,
    TALL_GRASS,
    WATER,
    LEDGE_NORTH,
    LEDGE_SOUTH,
    LEDGE_WEST,
    LEDGE_EAST,
    DOOR,
    SIGN,
    OUTSIDE,
    CLOSED_NORTH,
    CLOSED_SOUTH,
    CLOSED_WEST,
    CLOSED_EAST,
};

struct SimWanderBox{
    int x0 = 0, y0 = 0;
    int x1 = 0, y1 = 0;
};

struct SimNpc{
    std::string name;
    SimTile pos;
    std::optional<SimWanderBox> wander;
};

struct SimCoordEvent{
    std::string name;
    std::string map;
    std::vector<SimTile> tiles;
    std::string var;
    int value = 0;
};

struct SimItem{
    std::string item;
    SimTile pos;
};

struct SimTrainer{
    std::string name;
    std::string map;
    SimTile pos;
    SimDir facing = SimDir::SOUTH;
    int range = 4;
    bool optional_dodge = false;
};

struct SimMapData{
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<SimTileKind> cells;
    std::map<std::string, SimTile> points;
    std::vector<SimNpc> npcs;
    std::vector<SimCoordEvent> coord_events;
    std::vector<SimItem> items;
    bool indoor = false;
    bool cave = false;
    bool allow_run = true;

    bool contains(const SimTile& t) const{
        return t.x >= 0 && t.y >= 0 && t.x < width && t.y < height;
    }
    SimTileKind at(const SimTile& t) const{
        if (!contains(t)){
            return SimTileKind::OUTSIDE;
        }
        return cells[size_t(t.y) * size_t(width) + size_t(t.x)];
    }
    void set(const SimTile& t, SimTileKind k){
        if (contains(t)){
            cells[size_t(t.y) * size_t(width) + size_t(t.x)] = k;
        }
    }
};

struct SimWarpRule{
    std::string from_map;
    SimTile from_tile;
    std::optional<SimDir> required_dir; //  If set, warp only triggers moving in this direction.
    std::string to_map;
    SimTile to_tile;
    SimDir to_facing = SimDir::SOUTH;
    bool white_fade = false;            //  Cave exits use white fades; building doors use black fades.
};

struct SimLandmarkDef{
    std::string matcher_name;
    std::string map;
    double target_x = 0.0;              //  Player tile X where dx == 0 (in tile units).
    double target_y = 0.0;              //  Player tile Y where dy == 0 (in tile units).
    double max_range_x = 4.5;           //  Visible half-width in tiles.
    double max_range_y = 3.5;           //  Visible half-height in tiles.
    double coarse_range_x = 6.5;        //  Coarse acquisition half-width (if supported).
    double coarse_range_y = 5.5;
    bool has_coarse = false;
    double tol_x = 1.0 / 50.0;          //  at_target tolerance in screen fraction.
    double tol_y = 1.0 / 54.0;
    double screen_per_tile_x = 0.075;   //  ~13.3 tiles across 16:9 BDSP camera.
    double screen_per_tile_y = 0.115;   //  ~8.7 tiles tall under tilted camera.
    int landmark_count = 2;             //  Number of landmarks returned in OverworldMatchResult.
};

//  Repository of all loaded maps, overworld seamless connections, warps,
//  trainers, and geometric landmarks from Beginning -> Floaroma.
class Sim1717MapDB{
public:
    explicit Sim1717MapDB(const std::string& runtime_maps_dir);

    const SimMapData& map(const std::string& name) const;
    bool has_map(const std::string& name) const;

    //  Seamless overworld adjacency: if stepping off `map` at `next_tile`
    //  crosses into an adjacent overworld map, returns (new_map, local_tile).
    std::optional<std::pair<std::string, SimTile>> cross_border(
        const std::string& map_name, const SimTile& next_tile
    ) const;

    //  Lookup warp triggered when stepping onto `tile` in `map_name` while moving `dir`.
    const SimWarpRule* find_warp(
        const std::string& map_name, const SimTile& tile, SimDir dir
    ) const;

    //  All trainers active on `map_name`.
    std::vector<SimTrainer> trainers_on(const std::string& map_name) const;

    //  Landmark definition for `matcher_name`, or nullptr.
    const SimLandmarkDef* landmark(const std::string& matcher_name) const;

    //  World offset of an overworld map (for cross-map banner/distance calculations).
    std::optional<SimTile> overworld_offset(const std::string& map_name) const;

private:
    void load_runtime_map(const std::string& dir, const std::string& name, bool indoor, bool cave, bool allow_run);
    void build_synthetic_interiors();
    void build_overworld_offsets();
    void build_warps();
    void build_trainers();
    void build_landmarks();

    std::map<std::string, SimMapData> m_maps;
    std::map<std::string, SimTile> m_overworld_offsets;
    std::vector<SimWarpRule> m_warps;
    std::vector<SimTrainer> m_trainers;
    std::map<std::string, SimLandmarkDef> m_landmarks;
};

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
