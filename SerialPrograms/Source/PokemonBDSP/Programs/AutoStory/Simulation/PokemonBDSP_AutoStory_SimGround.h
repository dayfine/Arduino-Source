/*  BDSP AutoStory - Simulated ground
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Ground the story's maps lack that the console showed, for the story
 *  simulation to walk on: tiles past a map's edge, as a map of their own
 *  beside it.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SimGround_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SimGround_H

#include <memory>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Open `tiles` of `map` (a JSON map's name), in its own coordinates past
//  its edge, from story row `row` on.
struct SimGround{
    std::string map;
    std::vector<Pokemon::TilePos> tiles;
    std::string row;
};

//  The ground the story's maps lack.
const std::vector<SimGround>& sim_ground();

//  A copy of `world`, which holds `ground`'s map, with `ground`'s tiles
//  beside that map. Throws InternalProgramError for no tiles.
std::unique_ptr<StoryWorld> with_ground(const StoryWorld& world, const SimGround& ground);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
