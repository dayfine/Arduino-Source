/*  BDSP AutoStory - Map files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The program reads the story's maps from Packages' JSON files,
 *  PokemonBDSP/Maps/<name>.json, when it starts, checks each against the
 *  built-in copy the unit tests run on (test_map_json()), and plans on the
 *  loaded grids from then on (install_story_grids()).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_MapFiles_H
#define PokemonAutomation_PokemonBDSP_AutoStory_MapFiles_H

#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Where the program looks for the files: RESOURCE_PATH() + "PokemonBDSP/Maps/".
std::string story_map_folder();

//  Reads every map in `folder` through the shared loader and checks it
//  against the built-in copy. Throws FileException or ParseException naming
//  the file when one is missing, malformed or differs; else story_grid()
//  hands out the loaded grids from then on. Returns how many maps it read.
size_t load_story_map_files(const std::string& folder);



}
}
}
#endif
