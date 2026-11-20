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
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


namespace{

//  Throws ParseException naming `where` for an unknown field or a wrong value.
void read_signature_field(const std::string& field, const JsonValue& value, std::string_view where, StartSignature& signature){
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
            "{}: start_signature has \"{}\"; its fields are talk_prompt, wild_zone, party and minimap", where, field
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
        throw ParseException(std::format("{}: start_signature is an object of talk_prompt, wild_zone, party and minimap", where));
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

//  Adds `id` to the sets of `marked` the "execution" block's "save" and
//  "reload_when_chained" put it in; `previous`: the checkpoint before it.
void mark_checkpoint(
    const JsonObject& execution, std::string_view id, bool new_game, std::optional<std::string_view> previous, MarkedCheckpoints& marked
){
    const std::string_view where = "execution";
    if (const std::string* save = execution.get_string("save")){
        if (*save == "autosave"){
            marked.autosaves.emplace(id);
        }else if (*save == "unverified"){
            marked.unverified_autosaves.emplace(id);
        }else{
            throw ParseException(std::format("{}: save is \"autosave\", \"unverified\" or left out", where));
        }
    }
    if (execution.get_value("reload_when_chained") == nullptr){
        return;
    }
    if (!execution.get_boolean_throw("reload_when_chained", std::string(where))){
        throw ParseException(std::format("{}: reload_when_chained is true or left out", where));
    }
    if (new_game){
        throw ParseException(std::format("{}: reload_when_chained on a checkpoint with no save to reload", where));
    }
    if (!previous || !marked.autosaves.contains(*previous)){
        throw ParseException(std::format(
            "{}: reload_when_chained after a checkpoint that does not end on the game's autosave: the reload would load an older save",
            where
        ));
    }
    marked.reload_when_chained.emplace(id);
}

//  Checks the "execution" block, marks `checkpoint` in `marked`, and
//  checks that an autosave checkpoint names the step it autosaves in.
void read_execution(
    const JsonObject& execution, const StoryCheckpoint& checkpoint, std::optional<std::string_view> previous, MarkedCheckpoints& marked
){
    const std::string where = "execution";
    Pokemon::reject_unknown_keys(execution, {"save", "reload_when_chained", "autosave_step", "start_signature"});
    //  Read here only to fail the plan's load when malformed;
    //  builtin_checkpoint_starts() reads the starts.
    read_start_signature(execution, where);
    mark_checkpoint(execution, checkpoint.id, checkpoint_starts_without_save(checkpoint), previous, marked);
    const JsonValue* step = execution.get_value("autosave_step");
    const std::optional<std::string> autosave_step = step == nullptr ? std::nullopt : std::optional<std::string>(step->to_string_throw(where));
    if (const std::optional<std::string> error = autosave_step_error(checkpoint, marked.autosaves.contains(checkpoint.id), autosave_step)){
        throw ParseException(*error);
    }
}

} // namespace


StoryCheckpoint read_plza_checkpoint(
    std::string_view text, std::string_view source, Pokemon::StoryActions& actions, MarkedCheckpoints& marked,
    std::optional<std::string_view> previous
){
    const JsonValue file = parse_json(std::string(text));
    const JsonObject* plan = file.to_object() == nullptr ? nullptr : file.to_object()->get_object("checkpoint");
    for (const std::string& error : plan == nullptr ? std::vector<std::string>() : checkpoint_lead_in_errors(*plan)){
        throw ParseException(std::format("{}: {}", source, error));
    }
    StoryCheckpoint checkpoint = Pokemon::read_story_checkpoint(file, "plza", source, actions);
    try{
        read_execution(file.to_object_throw().get_object_throw("execution"), checkpoint, previous, marked);
    }catch (const ParseException& error){
        throw ParseException(std::format("{}: {}", source, error.message()));
    }
    return checkpoint;
}

StoryData load_plza_story(PlzaConsoleHooks& hooks){
    Pokemon::StoryActions actions = plza_story_actions(hooks);
    StoryData data;
    for (const auto& [name, text] : autostory_checkpoint_json()){
        const std::optional<std::string_view> previous = data.checkpoints.empty()
            ? std::nullopt
            : std::optional<std::string_view>(data.checkpoints.back().id);
        data.checkpoints.push_back(read_plza_checkpoint(text, name, actions, data, previous));
    }
    return data;
}

const StoryData& builtin_story_data(){
    static PlzaConsoleHooks no_hooks;
    static const StoryData data = load_plza_story(no_hooks);
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
    for (const Pokemon::StoryCheckpoint& checkpoint : builtin_story_data().checkpoints){
        const CheckpointStart* start = find_start(builtin_checkpoint_starts(), checkpoint.id);
        if (start != nullptr && (start->signature.wild_zone || start->signature.minimap)){
            return true;
        }
        if (checkpoint.id == asked){
            return false;
        }
    }
    return false;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
