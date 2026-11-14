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
using Pokemon::StoryFileRow;
using Pokemon::StoryRowExecution;


namespace{

//  A checkpoint file's checkpoint and its shared execution fields. `source`
//  names the file in parse errors.
StoryFileRow<StoryCheckpoint> read_checkpoint_file(std::string_view text, std::string_view source){
    StoryRowExecution read;
    const Pokemon::ExecutionReader read_execution = [&read](const JsonObject& execution, std::string_view where){
        //  Every PLZA file says how its row saves.
        execution.get_string_throw("save");
        read = Pokemon::read_story_row_execution(execution, where, {});
        if (read.dev_only){
            throw ParseException(std::format("{}: unknown execution field dev_only", where));
        }
    };
    StoryCheckpoint checkpoint = Pokemon::read_authored_checkpoint(
        parse_json(std::string(text)), "plza", source, read_execution, plza_story_specs()
    );
    return {.checkpoint = std::move(checkpoint), .execution = read};
}

} // namespace


const Pokemon::StoryFileReader<StoryCheckpoint>& plza_story_file_reader(){
    static const Pokemon::StoryFileReader<StoryCheckpoint> reader{
        .read = read_checkpoint_file,
        .lint = plza_story_lint,
        .starts_without_save = story_row_starts_without_save,
    };
    return reader;
}

StoryData load_story_data(std::span<const Pokemon::StoryDevFile> dev_files){
    return Pokemon::load_story_data(
        std::span<const Pokemon::EmbeddedJsonFile>(autostory_checkpoint_json()), dev_files, plza_story_file_reader()
    );
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
