/*  Story Dev Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  For lab tuning: checkpoint files on disk that a run reads in place of
 *  the built-in ones of the same name (load_story_data() in
 *  Pokemon_StoryFiles.h), so a walk can be retimed without a rebuild.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryDevFiles_H
#define PokemonAutomation_Pokemon_StoryDevFiles_H

#include <string>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  A checkpoint file read from disk.
struct StoryDevFile{
    std::string name;   //  "plza-02.01.plan.json"
    std::string path;
    std::string text;
};

//  The folder whose checkpoint files replace `game`'s built-in ones:
//  <user files>/AutoStoryDev/<game>/ (USER_FILE_PATH()).
std::string story_dev_folder(std::string_view game);

//  The <game>-*.plan.json files in `folder`, by name; none if it does not
//  exist.
std::vector<StoryDevFile> read_story_dev_files(std::string_view folder, std::string_view game);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
