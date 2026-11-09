/*  BDSP AutoStory - Simulated ground
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_SimGround.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::TileKind;
using Pokemon::TilePos;


const std::vector<SimGround>& sim_ground(){
    static const std::vector<SimGround> ground{
        //  Floaroma's flower field goes on a column west of the map: from
        //  below the old man (2, 26), taps west moved 3 tiles and the 4th
        //  bumped (westedge batch 2026-11-08).
        {.map = "FloaromaTown", .tiles = {{-1, 26}, {-1, 27}, {-1, 28}, {-1, 29}}, .row = "01.11"},
    };
    return ground;
}

namespace{

//  The offset `map` is placed at on `world`: a wall placed at offset (0, 0)
//  of a copy shows where that offset lands.
TilePos placed_at(const StoryWorld& world, const std::string& map){
    Pokemon::MapData probe;
    probe.name = "probe";
    probe.tiles = Pokemon::Grid<TileKind>{1, 1, {TileKind::WALL}};
    StoryWorld copy = world;
    copy.add(probe.name, probe, {0, 0});
    const TilePos corner = story_to_world(copy, map, {0, 0});
    const TilePos zero = copy.to_world(probe.name, {0, 0});
    return {corner.x - zero.x, corner.y - zero.y};
}

} // namespace

std::unique_ptr<StoryWorld> with_ground(const StoryWorld& world, const SimGround& ground){
    if (ground.tiles.empty()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No ground beside " + ground.map + ".");
    }
    TilePos low = ground.tiles.front();
    TilePos high = ground.tiles.front();
    for (const TilePos& pos : ground.tiles){
        low = {std::min(low.x, pos.x), std::min(low.y, pos.y)};
        high = {std::max(high.x, pos.x), std::max(high.y, pos.y)};
    }
    Pokemon::MapData strip;
    strip.name = ground.map + " ground";
    const int width = high.x - low.x + 1;
    const int height = high.y - low.y + 1;
    strip.tiles = Pokemon::Grid<TileKind>{width, height, std::vector<TileKind>((size_t)width * height, TileKind::WALL)};
    for (const TilePos& pos : ground.tiles){
        Pokemon::cell_at(strip.tiles, {pos.x - low.x, pos.y - low.y}) = TileKind::OPEN;
    }
    const TilePos corner = placed_at(world, ground.map);
    auto out = std::make_unique<StoryWorld>(world);
    out->add(strip.name, strip, {corner.x + low.x, corner.y + low.y});
    return out;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
