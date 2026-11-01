/*  BDSP AutoStory - Map files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's maps as JSON resources, PokemonBDSP/Maps/<name>.json in the
 *  shared map schema (Pokemon_MapData.h), written from the C++ tile tables
 *  by devtools/maps/bdsp_maps.py. The program loads them when it starts,
 *  checks each against its table cell for cell, and plans on the loaded
 *  grids; the offline tests, which have no resources, plan on the tables.
 *  Once the files are the source, the tables go.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_MapFiles_H
#define PokemonAutomation_PokemonBDSP_AutoStory_MapFiles_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


struct StoryMapFile{
    //  The file's name without ".json", and the map's "name" in it.
    const char* name;
    const Pokemon::TileGrid& (*table)();
};
const std::vector<StoryMapFile>& story_map_files();

//  Where the program looks for the files: RESOURCE_PATH() + "PokemonBDSP/Maps/".
std::string story_map_folder();

//  A map's tiles as a tile grid: walls, open ground, tall grass, south
//  ledges, doors and signs; a standing person ("npc" object) is an NPC tile.
//  Throws ParseException, naming the map, for what BDSP's grids have no
//  tile for (water, ledges other than south, items, wanderers).
Pokemon::TileGrid tile_grid_from_map(const Pokemon::MapData& map);

//  "(x, y): 'N' here, '.' there", or nothing when the grids are the same
//  size and equal cell for cell.
std::optional<std::string> grid_difference(const Pokemon::TileGrid& here, const Pokemon::TileGrid& there);

//  Reads every file in `folder` through the shared loader and checks it
//  against its table. Throws FileException or ParseException naming the
//  file when one is missing, malformed or differs; else story_grid() hands
//  out the loaded grids from then on.
void load_story_map_files(const std::string& folder);

//  The loaded grid of the file `name`, or its C++ table before
//  load_story_map_files() has run. Throws InternalProgramError for a name
//  not in story_map_files().
const Pokemon::TileGrid& story_grid(const std::string& name);



}
}
}
#endif
