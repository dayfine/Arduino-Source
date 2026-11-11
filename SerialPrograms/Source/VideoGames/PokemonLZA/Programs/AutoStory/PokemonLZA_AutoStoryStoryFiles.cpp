/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Data/Pokemon_AuthoredCheckpointJson.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


namespace{

//  A checkpoint file's checkpoint, and whether its execution says the game
//  autosaves at its end. `source` names the file in parse errors.
std::pair<StoryCheckpoint, bool> read_checkpoint_file(std::string_view text, std::string_view source){
    bool autosave = false;
    const Pokemon::ExecutionReader read_execution = [&autosave](const JsonObject& execution, std::string_view where){
        Pokemon::validate_json_fields(execution, {"save"}, where);
        const std::string& save = execution.get_string_throw("save");
        if (save != "autosave" && save != "menu"){
            throw ParseException(std::format("{}: save must be \"autosave\" or \"menu\", not {}", where, save));
        }
        autosave = save == "autosave";
    };
    StoryCheckpoint checkpoint = Pokemon::read_authored_checkpoint(
        parse_json(std::string(text)), "plza", source, read_execution, plza_story_specs()
    );
    return {std::move(checkpoint), autosave};
}

//  The dev file of `dev_files` named `name`, if any.
const StoryDevFile* find_dev_file(std::span<const StoryDevFile> dev_files, std::string_view name){
    for (const StoryDevFile& file : dev_files){
        if (file.name == name){
            return &file;
        }
    }
    return nullptr;
}

//  `dev`'s checkpoint, which replaces `builtin`'s: it must lint and keep its
//  id, so the run's start and end indices still mean the same rows.
std::pair<StoryCheckpoint, bool> read_dev_checkpoint(const StoryDevFile& dev, std::string_view builtin_text, std::string_view builtin_name){
    std::pair<StoryCheckpoint, bool> read;
    try{
        read = read_checkpoint_file(dev.text, dev.path);
    }catch (ParseException& e){
        //  Malformed JSON says nothing of where it came from.
        if (e.message().find(dev.path) == std::string::npos){
            throw ParseException(std::format("{}: {}", dev.path, e.message()));
        }
        throw;
    }
    const std::string builtin_id = read_checkpoint_file(builtin_text, builtin_name).first.id;
    if (read.first.id != builtin_id){
        throw ParseException(std::format("{}: checkpoint id {} does not match the built-in {}", dev.path, read.first.id, builtin_id));
    }
    const std::vector<std::string> problems = plza_story_lint(read.first);
    if (!problems.empty()){
        throw ParseException(std::format("{}: {}", dev.path, problems.front()));
    }
    return read;
}

} // namespace


StoryData load_story_data(std::span<const StoryDevFile> dev_files){
    for (const StoryDevFile& dev : dev_files){
        bool known = false;
        for (const auto& [name, text] : autostory_checkpoint_json()){
            known |= dev.name == std::format("{}.json", name);
        }
        if (!known){
            throw ParseException(std::format("{}: no built-in checkpoint file is named {}", dev.path, dev.name));
        }
    }
    StoryData data;
    for (const auto& [name, text] : autostory_checkpoint_json()){
        const std::string file_name = std::format("{}.json", name);
        const StoryDevFile* dev = find_dev_file(dev_files, file_name);
        auto [checkpoint, autosave] = dev == nullptr
            ? read_checkpoint_file(text, name)
            : read_dev_checkpoint(*dev, text, name);
        if (dev != nullptr){
            data.dev_files.emplace_back(file_name, dev->path);
        }
        if (autosave){
            data.autosaves.insert(checkpoint.id);
        }
        data.checkpoints.push_back(std::move(checkpoint));
    }
    return data;
}

const StoryData& builtin_story_data(){
    static const StoryData data = load_story_data({});
    return data;
}

const std::vector<StoryCheckpoint>& story_checkpoints(){
    return builtin_story_data().checkpoints;
}

const std::set<std::string, std::less<>>& story_autosave_rows(){
    return builtin_story_data().autosaves;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
