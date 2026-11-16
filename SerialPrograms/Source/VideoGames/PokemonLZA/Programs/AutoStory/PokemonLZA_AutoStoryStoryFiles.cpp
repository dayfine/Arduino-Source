/*  PokemonLZA AutoStory - Story files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
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

//  The execution's "backup_card", {"registered": N}, if it has one.
//  Throws ParseException naming `where` for a malformed one.
std::optional<BackupCard> read_backup_card(const JsonObject& execution, std::string_view where){
    const JsonValue* value = execution.get_value("backup_card");
    if (value == nullptr){
        return std::nullopt;
    }
    const JsonObject* card = value->to_object();
    const JsonValue* registered = card == nullptr ? nullptr : card->get_value("registered");
    int64_t count = 0;
    if (registered == nullptr || !registered->read_integer(count) || count < 0){
        throw ParseException(std::format("{}: backup_card is {{\"registered\": N}}, the backup card's Pokedex count", where));
    }
    return BackupCard{.registered = (size_t)count};
}

//  The checkpoint file `text`'s id and backup card, if its execution has
//  "backup_card". `source` names the file in parse errors.
std::optional<BackupStart> read_backup_start(std::string_view text, const std::string& source){
    const JsonValue file = parse_json(std::string(text));
    const JsonObject* root = file.to_object();
    const JsonObject* execution = root == nullptr ? nullptr : root->get_object("execution");
    const std::optional<BackupCard> card = execution == nullptr ? std::nullopt : read_backup_card(*execution, source);
    if (!card){
        return std::nullopt;
    }
    return BackupStart{.checkpoint = root->get_object_throw("checkpoint", source).get_string_throw("id", source), .card = *card};
}

//  A checkpoint file's checkpoint and its shared execution fields. `source`
//  names the file in parse errors. Throws ParseException too when the
//  checkpoint does not end on the autosave its "autosave_step" names
//  (autosave_step_error()).
StoryFileRow<StoryCheckpoint> read_checkpoint_file(std::string_view text, std::string_view source){
    StoryRowExecution read;
    std::optional<std::string> autosave_step;
    const Pokemon::ExecutionReader read_execution = [&read, &autosave_step](const JsonObject& execution, std::string_view where){
        read = Pokemon::read_story_row_execution(execution, where, /*game_fields=*/{"autosave_step", "backup_card"});
        read_backup_card(execution, where);
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

const std::vector<BackupStart>& builtin_backup_starts(){
    static const std::vector<BackupStart> starts = []{
        std::vector<BackupStart> read;
        for (const auto& [name, text] : autostory_checkpoint_json()){
            const std::optional<BackupStart> start = read_backup_start(text, name);
            if (!start){
                continue;
            }
            if (const std::optional<std::string> other = backup_checkpoint(read, start->card)){
                throw ParseException(std::format(
                    "{}: backup_card {} registered is {}'s too: a backup card must name one checkpoint", name, start->card.registered, *other
                ));
            }
            read.push_back(*start);
        }
        return read;
    }();
    return starts;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
