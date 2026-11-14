/*  PokemonLZA AutoStory - Dev checkpoint files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonLZA_AutoStoryDevFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::string story_dev_folder(){
    return std::format("{}AutoStoryDev/plza/", USER_FILE_PATH());
}

std::vector<StoryDevFile> read_story_dev_files(std::string_view folder){
    std::vector<StoryDevFile> files;
    for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(std::string(folder)))){
        const std::string name = path.filename().string();
        if (!name.starts_with("plza-") || !name.ends_with(".plan.json") || !Filesystem::is_regular_file(path)){
            continue;
        }
        files.push_back({.name = name, .path = path.string(), .text = file_to_string(path.string())});
    }
    return files;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
