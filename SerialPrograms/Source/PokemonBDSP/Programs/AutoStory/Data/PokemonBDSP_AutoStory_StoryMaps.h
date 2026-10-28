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

//  Route 202's trainers, in world tiles of story_maps().
std::vector<Pokemon::TrainerSight> story_trainer_sights();



}
}
}
#endif
