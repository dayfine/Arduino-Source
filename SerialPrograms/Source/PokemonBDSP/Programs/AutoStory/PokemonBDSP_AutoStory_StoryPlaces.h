/*  BDSP AutoStory - Story places
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the story's table and steps stand: tiles of the JSON maps
 *  ("Route202"), by map and point name (map_point(), map_region()). A walk
 *  from one map to another is planned on a story world that holds both: the
 *  outdoors from Twinleaf to Route 202, Jubilife, and each interior of
 *  00.01 to 00.04 on its own.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryPlaces_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryPlaces_H

#include <optional>
#include <string>
#include <vector>
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The point `point` of the map `map` (map_point()).
Pokemon::TilePos story_point(const std::string& map, const std::string& point);

//  The tiles of the region `region` of the map `map`: map_region(), and
//  the areas of Route 202 that the maps have no region for yet (where the
//  catch may leave us, the training grass, the exit to Jubilife). Throws
//  InternalProgramError for an unknown map or region.
std::vector<Pokemon::TilePos> story_region(const std::string& map, const std::string& region);

//  The story world a walk from `from` to `to` is planned on, or nothing if
//  no world holds both. Its maps are named as the game's location banners
//  name them ("Route 202"), which the walker reads.
const StoryWorld* story_plan_world(const std::string& from, const std::string& to);

//  The tile `local` of the map `map` in `world`.
Pokemon::TilePos story_to_world(const StoryWorld& world, const std::string& map, const Pokemon::TilePos& local);

//  The world tile `pos` as a tile of `map`, or nothing if it is not on `map`.
std::optional<Pokemon::TilePos> story_to_local(const StoryWorld& world, const std::string& map, const Pokemon::TilePos& pos);

//  The trainers a walk on `world` may meet (Route 202's), in its tiles.
std::vector<TrainerSight> story_trainers(const StoryWorld& world);



}
}
}
#endif
