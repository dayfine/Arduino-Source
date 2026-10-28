/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The maps the story walks through, joined on one grid.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H

#include "Pokemon/Navigation/Pokemon_MapSet.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


extern const char* const MAP_ROUTE_201;
extern const char* const MAP_SANDGEM_TOWN;
extern const char* const MAP_ROUTE_202;

//  Route 201, Sandgem Town and Route 202.
const Pokemon::MapSet& story_maps();

//  Whether a plan may cross this map. Routes are traced from the DP maps and
//  their tree walls carried over to BDSP; towns changed more in the remake, so
//  a town is crossed by screen-driven legs until the console has confirmed its
//  grid.
bool story_map_plannable(const std::string& map);

//  The open rows at the top of Route 202, in world tiles: walking north from
//  any of them enters Jubilife City.
bool story_at_jubilife_exit(const Pokemon::TilePos& world);

//  Route 202's trainers, in world tiles of story_maps().
std::vector<Pokemon::TrainerSight> story_trainer_sights();



}
}
}
#endif
