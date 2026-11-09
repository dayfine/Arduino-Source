/*  BDSP AutoStory - Story places
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the story's steps walk: poses are tiles of the JSON maps
 *  ("Route202"), named by map_point() and map_region(). A walk from one map
 *  to another is planned on a story world that holds both: the outdoors
 *  from Twinleaf to Route 202, Jubilife, Oreburgh, Route 203 into Jubilife,
 *  Jubilife up Route 204's south half, Route 204's north half into
 *  Floaroma, and each interior on its own.
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


//  The story world a walk from `from` to `to` is planned on, or nothing if
//  no world holds both. Its maps are named as the game's location banners
//  name them ("Route 202"), which the walker reads.
const StoryWorld* story_plan_world(const std::string& from, const std::string& to);

//  The location banner of the JSON map `map` ("Route 204" for both of its
//  halves), or `map` itself for one no banner list names.
const std::string& story_banner_name(const std::string& map);
//  The banner (story_banner_name()) a warp from the JSON map `from` onto
//  `to` shows, or "" if none: arriving in another town, route or cave shows
//  its name (out of the Gym, Oreburgh City's), arriving in a house, a lab,
//  a school or another floor of the same cave shows none.
std::string story_arrival_banner(const std::string& from, const std::string& to);

//  The tile `local` of the map `map` in `world`.
Pokemon::TilePos story_to_world(const StoryWorld& world, const std::string& map, const Pokemon::TilePos& local);

//  The world tile `pos` as a tile of `map`, or nothing if it is not on `map`.
std::optional<Pokemon::TilePos> story_to_local(const StoryWorld& world, const std::string& map, const Pokemon::TilePos& pos);

//  The trainers a walk on `world` may meet (Routes 202 to 204's), in its tiles.
std::vector<TrainerSight> story_trainers(const StoryWorld& world);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
