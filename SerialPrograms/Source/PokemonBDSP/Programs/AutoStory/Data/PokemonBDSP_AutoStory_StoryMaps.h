/*  BDSP AutoStory - Story maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The maps the story walks through, joined into story worlds, and the
 *  lines and entrances on them that the story's plans end at. The tiles
 *  and named points come from the JSON maps (PokemonBDSP_AutoStory_Maps.h);
 *  where the maps sit relative to each other the maps do not say, so it is
 *  here. Spots and trainers on the maps: StorySpots.h, StoryTrainers.h.
 */
// code-health: allow header_symbols -- each story map's world sits next to the lines and entrances on it that its plans end at

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryMaps_H

#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_StoryMapNames.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Route 201, Sandgem Town and Route 202.
const StoryWorld& story_maps();

//  Jubilife City on its own: 00.05 starts there after Dawn's scene, so it is
//  not joined to Route 202.
const StoryWorld& jubilife_maps();
//  Route 203 on its own (01.06), and whether a tile is on row 18 between the
//  two cliffs: row 18 runs straight west from there under the west cliff
//  into Jubilife, so the plan ends there (no leg may leave the map) and a
//  walk west until Jubilife's banner shows goes on from there.
const StoryWorld& route203_maps();
//  Returns whether given tile lies on the west line towards Jubilife.
bool on_route203_west_line(const Pokemon::TilePos& pos);
//  Jubilife City's north-east corner (01.06), at its own tile coordinates:
//  the rest of the city is walled off, so the plan searches only the part
//  the walk needs. Where Route 203's road enters it, and whether a tile is
//  on the north road below the plaza: walking north from there goes up the
//  road to Route 204 (and Rowan's scene).
const StoryWorld& jubilife_east_maps();
//  Returns entrance coordinates for east entrance of Jubilife.
const std::vector<Pokemon::TilePos>& jubilife_east_entrance();
//  Returns whether given tile lies on the north road towards Route 204.
bool on_jubilife_north_road(const Pokemon::TilePos& pos);
//  The Oreburgh Mine's coal room on its own (01.03).
const StoryWorld& oreburgh_mine_maps();

//  The Ravaged Path on its own (01.05), with the rocks in `smashed` open.
StoryWorld ravaged_path_maps(const std::vector<Pokemon::TilePos>& smashed = {});

//  Route 204's north half on its own (01.05), and whether a tile is on the
//  path north into Floaroma.
const StoryWorld& route204_north_maps();
//  Returns whether given tile lies within the Floaroma entrance path.
bool in_floaroma_path(const Pokemon::TilePos& pos);

//  Route 204's south half on its own (01.08): it joins the north half only
//  through the Ravaged Path, so the two are separate maps.
const StoryWorld& route204_south_maps();

//  Oreburgh City alone (01.04, 01.05), and Floaroma Town alone (01.11).
const StoryWorld& oreburgh_city_maps();
//  Returns StoryWorld instance containing Floaroma Town alone.
const StoryWorld& floaroma_maps();

//  Whether a plan may cross this map. Towns are crossed by screen-driven
//  legs until the console has confirmed their maps.
bool story_map_plannable(const std::string& map);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
