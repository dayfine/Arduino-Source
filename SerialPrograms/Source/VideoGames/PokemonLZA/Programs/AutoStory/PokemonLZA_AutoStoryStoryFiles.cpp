/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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
//  names the file in parse errors. Throws ParseException too when the
//  checkpoint does not end on the autosave its "autosave_step" names
//  (autosave_step_error()).
StoryFileRow<StoryCheckpoint> read_checkpoint_file(std::string_view text, std::string_view source){
    StoryRowExecution read;
    std::optional<std::string> autosave_step;
    const Pokemon::ExecutionReader read_execution = [&read, &autosave_step](const JsonObject& execution, std::string_view where){
        read = Pokemon::read_story_row_execution(execution, where, /*game_fields=*/{"autosave_step"});
        if (const JsonValue* step = execution.get_value("autosave_step")){
            const std::string* name = step->to_string();
            if (name == nullptr){
                throw ParseException(std::format("{}: autosave_step is a step's id", where));
            }
            autosave_step = *name;
        }
    };
    StoryCheckpoint checkpoint = Pokemon::read_authored_checkpoint(
        parse_json(std::string(text)), "plza", source, read_execution, plza_story_specs()
    );
    if (const std::optional<std::string> error = autosave_step_error(checkpoint, read.save, autosave_step)){
        throw ParseException(std::format("{}: {}", source, *error));
    }
    return {.checkpoint = std::move(checkpoint), .execution = read};
}

//  How the shared loader reads a PLZA checkpoint file: against
//  plza_story_specs(), its execution with the shared "save" and
//  "reload_when_chained" and PLZA's "autosave_step". 01.00 (story_row_starts_without_save()) has no
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
