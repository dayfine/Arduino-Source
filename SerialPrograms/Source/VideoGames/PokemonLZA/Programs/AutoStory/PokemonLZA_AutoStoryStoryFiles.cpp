/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <string_view>
#include <utility>
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
        read = Pokemon::read_story_row_execution(execution, where, /*game_fields=*/{});
    };
    StoryCheckpoint checkpoint = Pokemon::read_authored_checkpoint(
        parse_json(std::string(text)), "plza", source, read_execution, plza_story_specs()
    );
    return {.checkpoint = std::move(checkpoint), .execution = read};
}

//  How the shared loader reads a PLZA checkpoint file: against
//  plza_story_specs(), its execution with only the shared "save" and
//  "reload_when_chained". 01.00 (story_row_starts_without_save()) has no
//  save to reload.
const Pokemon::StoryFileReader<StoryCheckpoint>& plza_story_file_reader(){
    static const Pokemon::StoryFileReader<StoryCheckpoint> reader{
        .read = read_checkpoint_file,
        .starts_without_save = story_row_starts_without_save,
    };
    return reader;
}

} // namespace


const StoryData& builtin_story_data(){
    static const StoryData data = Pokemon::load_story_data(
        std::span<const Pokemon::EmbeddedJsonFile>(autostory_checkpoint_json()), plza_story_file_reader()
    );
    return data;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
