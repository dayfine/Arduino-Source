/*  Story Dev Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "Pokemon_StoryDevFiles.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string story_dev_folder(std::string_view game){
    return std::format("{}AutoStoryDev/{}/", USER_FILE_PATH(), game);
}

std::vector<StoryDevFile> read_story_dev_files(std::string_view folder, std::string_view game){
    const Filesystem::Path directory(std::string{folder});
    std::vector<StoryDevFile> files;
    const std::string prefix = std::format("{}-", game);
    for (const Filesystem::Path& path : Filesystem::list_directory(directory)){
        const std::string name = path.filename().string();
        if (!name.starts_with(prefix) || !name.ends_with(".plan.json") || !Filesystem::is_regular_file(path)){
            continue;
        }
        files.push_back({.name = name, .path = path.string(), .text = file_to_string(path.string())});
    }
    return files;
}



} // namespace Pokemon
} // namespace PokemonAutomation
