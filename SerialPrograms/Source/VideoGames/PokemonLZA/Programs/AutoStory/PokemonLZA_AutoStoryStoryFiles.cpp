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

//  Throws ParseException naming `where` for an unknown field or a wrong value.
void read_signature_field(const std::string& field, const JsonValue& value, std::string_view where, StartSignature& signature){
    if (field == "registered"){
        int64_t count = 0;
        if (!value.read_integer(count) || count < 0){
            throw ParseException(std::format("{}: start_signature's registered is the backup card's Pokedex count", where));
        }
        signature.registered = (size_t)count;
        return;
    }
    if (field == "party"){
        int64_t slots = 0;
        if (!value.read_integer(slots) || slots < 1 || slots > 6){
            throw ParseException(std::format("{}: start_signature's party is the party bar's filled slots, 1 to 6", where));
        }
        signature.party = (size_t)slots;
        return;
    }
    if (field == "minimap"){
        const std::string* name = value.to_string();
        if (name == nullptr || name->empty()){
            throw ParseException(std::format("{}: start_signature's minimap is a minimap reference's name", where));
        }
        signature.minimap = *name;
        return;
    }
    bool flag = false;
    if (field != "talk_prompt" && field != "wild_zone"){
        throw ParseException(std::format(
            "{}: start_signature has \"{}\"; its fields are registered, talk_prompt, wild_zone, party and minimap", where, field
        ));
    }
    if (!value.read_boolean(flag)){
        throw ParseException(std::format("{}: start_signature's {} is true or false", where, field));
    }
    (field == "talk_prompt" ? signature.talk_prompt : signature.wild_zone) = flag;
}

//  Throws ParseException naming `where` for a malformed or empty one.
std::optional<StartSignature> read_start_signature(const JsonObject& execution, std::string_view where){
    const JsonValue* value = execution.get_value("start_signature");
    if (value == nullptr){
        return std::nullopt;
    }
    const JsonObject* fields = value->to_object();
    if (fields == nullptr || fields->begin() == fields->end()){
        throw ParseException(std::format("{}: start_signature is an object of registered, talk_prompt, wild_zone, party and minimap", where));
    }
    StartSignature signature;
    for (const auto& [field, field_value] : *fields){
        read_signature_field(field, field_value, where, signature);
    }
    return signature;
}

std::optional<CheckpointStart> read_checkpoint_start(std::string_view text, const std::string& source){
    const JsonValue file = parse_json(std::string(text));
    const JsonObject* root = file.to_object();
    const JsonObject* execution = root == nullptr ? nullptr : root->get_object("execution");
    const std::optional<StartSignature> signature = execution == nullptr ? std::nullopt : read_start_signature(*execution, source);
    if (!signature){
        return std::nullopt;
    }
    return CheckpointStart{.checkpoint = root->get_object_throw("checkpoint", source).get_string_throw("id", source), .signature = *signature};
}

//  Also throws ParseException when the checkpoint does not end on the
//  autosave its "autosave_step" names.
StoryFileRow<StoryCheckpoint> read_checkpoint_file(std::string_view text, std::string_view source){
    StoryRowExecution read;
    std::optional<std::string> autosave_step;
    const Pokemon::ExecutionReader read_execution = [&read, &autosave_step](const JsonObject& execution, std::string_view where){
        read = Pokemon::read_story_row_execution(execution, where, /*game_fields=*/{"autosave_step", "start_signature"});
        read_start_signature(execution, where);
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

//  01.00a and 01.00b have no save to reload.
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

const std::vector<CheckpointStart>& builtin_checkpoint_starts(){
    static const std::vector<CheckpointStart> starts = []{
        std::vector<CheckpointStart> read;
        for (const auto& [name, text] : autostory_checkpoint_json()){
            const std::optional<CheckpointStart> start = read_checkpoint_start(text, name);
            if (start){
                read.push_back(*start);
            }
        }
        return read;
    }();
    return starts;
}

bool builtin_start_reads_map(std::string_view asked){
    for (const Pokemon::StoryCheckpoint& row : builtin_story_data().checkpoints){
        const CheckpointStart* start = find_start(builtin_checkpoint_starts(), row.id);
        if (start != nullptr && (start->signature.wild_zone || start->signature.minimap)){
            return true;
        }
        if (row.id == asked){
            return false;
        }
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
