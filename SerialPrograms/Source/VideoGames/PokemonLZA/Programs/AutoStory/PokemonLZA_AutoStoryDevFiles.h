/*  PokemonLZA AutoStory - Dev checkpoint files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  For lab tuning: checkpoint files on disk that a run reads in place of
 *  the built-in ones of the same name (load_story_data()), so a walk can be
 *  retimed without a rebuild.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDevFiles_H
#define PokemonAutomation_PokemonLZA_AutoStoryDevFiles_H

#include <string>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A checkpoint file read from disk.
struct StoryDevFile{
    std::string name;   //  "plza-02.01.plan.json"
    std::string path;
    std::string text;
};

//  The folder whose checkpoint files replace the built-in ones:
//  <user files>/AutoStoryDev/plza/ (USER_FILE_PATH()).
std::string story_dev_folder();

//  The plza-*.plan.json files in `folder`, by name; none if it does not
//  exist.
std::vector<StoryDevFile> read_story_dev_files(std::string_view folder);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
