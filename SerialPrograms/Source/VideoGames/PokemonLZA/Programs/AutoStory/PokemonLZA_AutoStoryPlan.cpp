/*  PokemonLZA AutoStory - Plan
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
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


std::string_view checkpoint_first_action(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.front().action);
}

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

//  Checks "save" ("autosave" | "unverified") and "reload_when_chained",
//  adding `id` to `marked`'s sets when set.
void mark_checkpoint(
    const JsonObject& execution, std::string_view id,
    std::optional<std::string_view> previous, MarkedCheckpoints& marked
){
    const std::string where = "execution";
    const std::string& save = execution.get_string_throw("save", where);
    if (save == "autosave"){
        marked.autosaves.emplace(id);
    }else if (save != "unverified"){
        throw ParseException(std::format("{}: save must be \"autosave\" or \"unverified\"", where));
    }
    const JsonValue* reload = execution.get_value("reload_when_chained");
    if (reload == nullptr){
        return;
    }
    if (!reload->to_boolean_throw(where)){
        return;
    }
    if (!previous || !marked.autosaves.contains(*previous)){
        throw ParseException(std::format(
            "{}: reload_when_chained after a checkpoint that does not end on the game's autosave: the reload would load an older save",
            where
        ));
    }
    marked.reload_when_chained.emplace(id);
}

//  Checks the "execution" block, records start signature and marks in `data`,
//  and checks that an autosave checkpoint names the step it autosaves in.
void read_execution(
    const JsonObject& execution, const StoryCheckpoint& checkpoint, std::optional<std::string_view> previous, StoryData& data
){
    const std::string where = "execution";
    Pokemon::reject_unknown_keys(execution, {"save", "reload_when_chained", "autosave_step", "start_signature"});
    if (std::optional<StartSignature> signature = read_start_signature(execution, where)){
        data.starts.push_back(CheckpointStart{.checkpoint = checkpoint.id, .signature = std::move(*signature)});
    }
    mark_checkpoint(execution, checkpoint.id, previous, data);
    const JsonValue* step = execution.get_value("autosave_step");
    const std::optional<std::string> autosave_step = step == nullptr ? std::nullopt : std::optional<std::string>(step->to_string_throw(where));
    if (const std::optional<std::string> error = autosave_step_error(checkpoint, data.autosaves.contains(checkpoint.id), autosave_step)){
        throw ParseException(*error);
    }
}

StoryCheckpoint read_plza_checkpoint(
    std::string_view text, std::string_view source, Pokemon::StoryActions& actions, StoryData& data,
    std::optional<std::string_view> previous
){
    const JsonValue file = parse_json(std::string(text));
    StoryCheckpoint checkpoint = Pokemon::read_story_checkpoint(file, "plza", source, actions);
    try{
        read_execution(file.to_object_throw().get_object_throw("execution"), checkpoint, previous, data);
    }catch (const ParseException& error){
        throw ParseException(std::format("{}: {}", source, error.message()));
    }
    return checkpoint;
}

} // namespace


StoryData load_plza_story(){
    Pokemon::StoryActions actions = plza_story_actions();
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
    static const StoryData data = load_plza_story();
    return data;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
