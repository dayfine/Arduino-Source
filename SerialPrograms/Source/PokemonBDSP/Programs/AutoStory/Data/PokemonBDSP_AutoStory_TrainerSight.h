/*  BDSP AutoStory - Trainer sight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which tiles a trainer sees, and which legs of a walk cross them.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TrainerSight_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TrainerSight_H

#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A trainer sees the tiles in a straight line in front of them, up to
//  `range` tiles or the first tile the player cannot stand on. The maps
//  have no sight ranges, so story code lists them.
struct TrainerSight{
    std::string name;
    Pokemon::TilePos pos;
    Pokemon::Direction facing = Pokemon::Direction::NORTH;
    int range = 4;
};
//  Computes straight line-of-sight tiles for a trainer.
std::vector<Pokemon::TilePos> sight_tiles(const StoryWorld& world, const TrainerSight& trainer);

//  For each leg, the trainers whose sight line it enters.
std::vector<std::vector<std::string>> legs_crossing_sight(
    const StoryWorld& world,
    const Pokemon::TilePos& from,
    const std::vector<TileLeg>& legs,
    const std::vector<TrainerSight>& trainers
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
